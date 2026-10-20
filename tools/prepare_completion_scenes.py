"""원작 던전 정의·종료 스크립트에서 선택 완료 장소와 맵 이름을 추출한다."""
import argparse
from copy import copy
import hashlib
import json
from pathlib import Path
import re
import struct

from tools.package_format import encode_package


def prepare(root, output):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.util import get_ppmdu_config_for_rom, get_binary_from_rom
    from skytemple_files.hardcoded.dungeons import HardcodedDungeons
    if output.exists():
        raise ValueError('choose a new output directory')
    digest = lambda data: hashlib.sha256(data).hexdigest()
    source = root / 'assets/original/source/user-supplied.nds'
    before = digest(source.read_bytes())
    rom = NintendoDSRom.fromFile(str(source))
    config = get_ppmdu_config_for_rom(rom)
    definitions = HardcodedDungeons.get_dungeon_list(get_binary_from_rom(rom, config.bin_sections.arm9), config)
    mappa = FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'))
    backgrounds = FileType.BG_LIST_DAT.deserialize(rom.getFileByName('MAP_BG/bg_list.dat'))
    byte_codec = copy(config)
    byte_codec.string_encoding = 'latin-1'  # Only opcodes/positions; no dialogue decoding claim.
    # The runtime currently ships Test Dungeon0 and Upper Steam Cave15.
    dungeon, terminal, level = 15, 16, 'D10P41A'
    definition, end = definitions[dungeon], definitions[terminal]
    fixed = mappa.floor_lists[end.mappa_index][end.start_after].layout.fixed_floor_id
    if (definition.mappa_index, definition.start_after, definition.number_floors,
        end.mappa_index, end.start_after, end.number_floors, fixed) != (12, 8, 7, 12, 15, 1, 3):
        raise ValueError('original Upper Steam Cave -> Steam Cave Peak mapping changed')
    script_path = f'SCRIPT/{level}/m10a0901.ssb'
    script_raw = rom.getFileByName(script_path)
    script = FileType.SSB.deserialize(script_raw, byte_codec).to_ssb_script()[0]
    if 'back_SetDungeonBanner(16, 0)' not in script or f'back_SetGround(LEVEL_{level})' not in script:
        raise ValueError('original completion scene/banner relationship changed')
    targets, owner = {}, None
    for line in script.splitlines():
        match = re.search(r'lives\(ACTOR_(PLAYER|ATTENDANT1)\)', line)
        if match:
            owner = match[1]
        elif 'lives(' in line:
            owner = None
        match = re.search(r"MovePositionMark\([^,]+, Position<[^,]+, ([\d.]+), ([\d.]+)>", line)
        if match and owner and owner not in targets:
            targets[owner] = [round(float(match[1])*8), round(float(match[2])*8)]
    layout_path = f'SCRIPT/{level}/m10a0901.ssa'
    layout_raw = rom.getFileByName(layout_path)
    layout = FileType.SSA.deserialize(layout_raw, config.script_data)
    def position(actor):
        return [actor.pos.x_relative*8+(4 if actor.pos.x_offset>=2 else 0),
                actor.pos.y_relative*8+(4 if actor.pos.y_offset>=2 else 0)]
    actors = {a.actor.id: a for a in layout.layer_list[0].actors}
    entry_points = [position(actors[0]), position(actors[10])]
    end_points = [targets['PLAYER'], targets['ATTENDANT1']]
    bg = next(e for e in backgrounds.level if e.bma_name == level)
    refs = []
    def load(kind, filename, extension):
        path = f'MAP_BG/{filename.lower()}.{extension}'
        raw = rom.getFileByName(path)
        refs.append({'path': path, 'sha256': digest(raw)})
        return kind.deserialize(raw)
    bma = load(FileType.BMA, bg.bma_name, 'bma')
    bpc = load(FileType.BPC, bg.bpc_name, 'bpc')
    bpl = load(FileType.BPL, bg.bpl_name, 'bpl')
    bpas = [load(FileType.BPA, name, 'bpa') if name else None for name in bg.bpa_names]
    # Pinned SkyTemple's palette iterator needs materializing for Pillow 12 crop.
    original_chunks = bpc.chunks_animated_to_pil
    def materialized_chunks(*args, **kwargs):
        images = original_chunks(*args, **kwargs)
        for chunk in images:
            if chunk.palette and not isinstance(chunk.palette.palette, (bytes, bytearray, list)):
                chunk.palette.palette = bytes(chunk.palette.palette)
        return images
    bpc.chunks_animated_to_pil = materialized_chunks
    image = bma.to_pil(bpc, bpl, bpas, include_collision=False,
                       include_unknown_data_block=False, pal_ani=False, single_frame=True)[0].convert('RGBA')
    image.putalpha(255)
    if any(not (0 <= x < image.width and 0 <= y < image.height) for x, y in entry_points+end_points):
        raise ValueError('original completion actor position outside background')
    if digest(source.read_bytes()) != before:
        raise ValueError('ROM changed')
    output.mkdir(parents=True)
    image.save(output / 'peak.png')
    provenance = {'romSha256': before, 'dungeonId': dungeon, 'terminalDungeonId': terminal,
                  'mappaGroup': 12, 'groupLastFloor': 15, 'fixedFloorId': fixed, 'level': level,
                  'script': {'path': script_path, 'sha256': digest(script_raw)},
                  'layout': {'path': layout_path, 'sha256': digest(layout_raw)},
                  'entryPoints': entry_points, 'endPoints': end_points, 'backgroundSources': refs,
                  'rgbaSha256': digest(image.tobytes()),
                  'scope': 'original static background, actor placement and initial walking targets; generic completion replaces original story/boss event'}
    png = (output / 'peak.png').read_bytes()
    package = {'assetVersion': 1, 'tickHz': 60, 'width': image.width, 'height': image.height,
               'pngBytes': len(png), 'sourceKind': 1, 'atlasName': 'peak.png',
               'pngSha256': digest(png), 'assetKey': 'rom:EoS_NA:completion:15',
               'provenance': json.dumps(provenance, sort_keys=True),
               'clips': [{'id': 0, 'name': level, 'rows': [[{'rect': [0, 0, image.width, image.height],
                   'anchor': [0, 0], 'shadow': [0, 0], 'ticks': 1, 'markers': []}]]}]}
    (output / 'peak.mdpa').write_bytes(encode_package(package))
    localized = {0: '테스트 던전', 15: '뜨거운 동굴 상층', 16: '뜨거운 동굴 정상'}
    names = [(d.id, localized.get(d.id, d.name)) for d in config.dungeon_data.dungeons if d.id < len(definitions)]
    body = bytearray(struct.pack('<2I', 0x4345444D, 1))
    def u32(value): body.extend(struct.pack('<I', value))
    def string(value):
        raw = value.encode('utf-8')
        if len(raw) > 256:
            raise ValueError('completion registry string exceeds limit')
        u32(len(raw)); body.extend(raw)
    string(before); u32(len(names))
    for identity, name in names:
        u32(identity); string(name); u32(identity == dungeon)
        if identity == dungeon:
            u32(terminal); string(localized[terminal]); string('peak.mdpa')
            u32(image.width); u32(image.height)
            for point in entry_points+end_points:
                for value in point: u32(value)
    (output / 'destinations.mdec').write_bytes(body)
    provenance['testDungeon0'] = 'original Test Dungeon; no official destination; use completion text in current map'
    (output / 'provenance.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    (output / 'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original ROM completion background\n', encoding='utf-8')
    return {'romUnchanged': True, 'mapNames': len(names), 'destination': provenance,
            'imageSize': image.size, 'selectedRuntimeDungeon': dungeon}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.root, args.out), ensure_ascii=False, indent=2))
