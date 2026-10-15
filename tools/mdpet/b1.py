"""B-1 offline catalogue and original DMA/DPC bridge; A extraction is reused."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import struct
import zipfile

from .catalogue import build_floor
from .common import canonical, file_hash, sha256, staging, write_json
from .extractor import checked_pack, smoke
from tools.package_format import encode_package


def load_catalogue(path: Path):
    with zipfile.ZipFile(path) as archive:
        name = 'a1-rom-final-01/catalogue.json'
        if archive.getinfo(name).file_size > 32 * 1024 * 1024:
            raise ValueError('catalogue byte limit')
        return json.loads(archive.read(name))


def write_profiles(profiles):
    out = bytearray()
    def u32(n): out.extend(struct.pack('<I', n))
    def u64(n): out.extend(struct.pack('<Q', n))
    def string(s):
        b = s.encode('utf-8'); u32(len(b)); out.extend(b)
    def weights(rows):
        u32(len(rows))
        for row in rows: u32(row['originalId']); u64(row['relativeWeight'])
    u32(len(profiles))
    for p in profiles:
        key = p['key']; string(key['edition']); u32(key['dungeonId']); u32(key['localFloor'])
        u32(p['tilesetId']); u32(p['groupFloor'])
        context = next(c for c in p['itemContexts'] if c['context'] == 'floor_items')
        if context['categories']['guaranteed']:
            raise ValueError('guaranteed category policy unresolved; refusing to change weights')
        cats = context['categories']['entries']
        conditional = {c['categoryId']: c for c in context['conditionalItems']}
        u32(len(cats))
        for c in cats:
            u32(c['originalId']); u64(c['relativeWeight'])
            rows = conditional.get(c['originalId'], {}).get('entries', [])
            if c['relativeWeight'] and not any(r['relativeWeight'] for r in rows):
                raise ValueError('positive category has no conditional items')
            weights(rows)
        weights(p['monsters']['normal']['entries'])
        u32(len(context['guaranteed']))
        for g in context['guaranteed']: u32(g['recordIndex'])
    return bytes(out)


def export_tileset(pack, tileset_id: int, output: Path, rom_hash: str):
    from PIL import Image
    dma, dpc, dpci, dpl = (pack[offset + tileset_id] for offset in (170, 340, 510, 680))
    chunks = dpc.chunks_to_pil(dpci, dpl.palettes, 1)
    count = len(dpc.chunks)
    if chunks.size != (24, count * 24) or not 0 < count <= 4096:
        raise ValueError('invalid chunk surface dimensions')
    atlas = Image.new('RGBA', (24 * 20, 24 * ((count + 19) // 20)))
    frames = []
    for index in range(count):
        chunk = chunks.crop((0, index * 24, 24, index * 24 + 24))
        rgba = chunk.convert('RGBA')
        # Palette index 0 of each subpalette is transparent, including opaque black elsewhere.
        rgba.putalpha(Image.frombytes('L', chunk.size, bytes(0 if px % 16 == 0 else 255 for px in chunk.tobytes())))
        x, y = (index % 20) * 24, (index // 20) * 24
        atlas.paste(rgba, (x, y))
        if atlas.crop((x, y, x + 24, y + 24)).tobytes() != rgba.tobytes():
            raise ValueError('chunk atlas pixel mismatch')
        frames.append({'rect': [x, y, 24, 24], 'anchor': [0, 0], 'shadow': [0, 0], 'ticks': 1, 'markers': []})
    png_name = f'tileset-{tileset_id}.png'; mdpa_name = f'tileset-{tileset_id}.mdpa'
    atlas.save(output / png_name)
    refs = [{'slot': offset + tileset_id, 'name': pack.get_filename(offset + tileset_id),
             'sha256': sha256(pack.get_raw(pack.get_filename(offset + tileset_id)))}
            for offset in (0, 170, 340, 510, 680)]
    provenance = {'sourceKind': 'rom', 'romHash': rom_hash, 'tilesetId': tileset_id, 'refs': refs,
                  'paletteAnimation': 'pending; static DPL palette only',
                  'credit': 'Nintendo / Pokémon / CHUNSOFT'}
    mdpa = {'assetVersion': 1, 'tickHz': 60, 'width': atlas.width, 'height': atlas.height,
            'pngBytes': (output / png_name).stat().st_size, 'sourceKind': 1,
            'atlasName': png_name, 'pngSha256': file_hash(output / png_name),
            'assetKey': f'rom:tileset:{tileset_id}', 'provenance': json.dumps(provenance, sort_keys=True),
            'clips': [{'id': 0, 'name': 'Chunks', 'rows': [frames]}]}
    (output / mdpa_name).write_bytes(encode_package(mdpa))
    mappings = [int(v) for t in range(3) for n in range(256) for v in dma.get(t, n)]
    if len(mappings) != 2304 or any(not 0 <= m < count for m in mappings):
        raise ValueError('DMA mapping outside DPC')
    return {'id': tileset_id, 'count': count, 'package': mdpa_name, 'mappings': mappings,
            'provenance': provenance, 'atlasHash': mdpa['pngSha256']}


def build(rom_path: Path, catalogue_zip: Path, out: Path, dungeon_id: int):
    smoke()  # Same pinned SkyTemple API/config as A.
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.util import get_ppmdu_config_for_rom, get_binary_from_rom
    from skytemple_files.hardcoded.dungeons import HardcodedDungeons
    before = file_hash(rom_path)
    catalogue = load_catalogue(catalogue_zip)
    if catalogue['identity']['romHash'] != before:
        raise ValueError('A catalogue does not match ROM')
    rom = NintendoDSRom.fromFile(str(rom_path)); config = get_ppmdu_config_for_rom(rom)
    if config.game_edition != catalogue['identity']['edition'] or config.game_version != 'EoS':
        raise ValueError('ROM edition mismatch')
    arm9 = get_binary_from_rom(rom, config.bin_sections.arm9)
    definitions = HardcodedDungeons.get_dungeon_list(arm9, config)
    if not 0 <= dungeon_id < len(definitions): raise ValueError('dungeon outside general definitions')
    definition = definitions[dungeon_id]
    if not 0 < definition.number_floors <= 128: raise ValueError('invalid selected floor count')
    mappa = FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'))
    members = {int(k): v for k, v in catalogue['itemCategoryMembers'].items()}
    profiles = [build_floor(config.game_edition, dungeon_id, i, definition, mappa.floor_lists,
                            catalogue['pokemon'], catalogue['items'], members)
                for i in range(definition.number_floors)]
    for existing in catalogue['floors']:
        if existing['key']['dungeonId'] == dungeon_id:
            if existing != profiles[existing['key']['localFloor']]:
                raise ValueError('B profile differs from existing A catalogue')
    raw_pack = rom.getFileByName('DUNGEON/dungeon.bin'); checked_pack(raw_pack)
    pack = FileType.DUNGEON_BIN.deserialize(raw_pack, static_data=config)
    with staging(out) as temp:
        tilesets = [export_tileset(pack, t, temp, before) for t in sorted({p['tilesetId'] for p in profiles})]
        body = bytearray(write_profiles(profiles)); body.extend(struct.pack('<I', len(tilesets)))
        def string(s):
            b = s.encode('utf-8'); return struct.pack('<I',len(b)) + b
        for t in tilesets:
            body.extend(struct.pack('<II',t['id'],t['count'])); body.extend(string(t['package']))
            body.extend(struct.pack('<' + 'I' * 2304, *t['mappings']))
        # Version binds profiles/rules and all exported pixel/metadata files.
        identity = bytes(body) + canonical({p.name: file_hash(p) for p in sorted(temp.iterdir())})
        version = sha256(identity)
        (temp / 'exploration.mdpe').write_bytes(struct.pack('<II',0x4550444d,1) + string(version) + string(before) + body)
        write_json(temp / 'catalogue.json', {'schemaVersion': 1, 'identity': catalogue['identity'],
                                            'floors': profiles, 'items': catalogue['items'], 'pokemon': catalogue['pokemon']})
        after = file_hash(rom_path)
        if after != before: raise ValueError('ROM changed during tile conversion')
        report = {'status': 'pass', 'assetVersion': version, 'romHashBefore': before, 'romHashAfter': after,
                  'aCatalogueHash': file_hash(catalogue_zip), 'dungeonId': dungeon_id,
                  'floorCount': len(profiles), 'tilesets': tilesets,
                  'stairsGraphic': 'unresolved; logical marker only', 'paletteAnimation': 'pending',
                  'friendGraphics': 'missing species remain logical markers; original weights preserved',
                  'a3Remaining': 'pending'}
        write_json(temp / 'conversion.json', report)
        (temp / 'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT original user-supplied assets.\nOffline parsers: SkyTemple-files (GPL-3.0-or-later).\n',encoding='utf-8')
    return report


def main():
    p=argparse.ArgumentParser();p.add_argument('--rom',type=Path,required=True)
    p.add_argument('--catalogue-zip',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--dungeon',type=int,default=0);a=p.parse_args()
    report=build(a.rom,a.catalogue_zip,a.out,a.dungeon)
    print(json.dumps({k:v for k,v in report.items() if k!='tilesets'},ensure_ascii=False))
if __name__=='__main__': main()
