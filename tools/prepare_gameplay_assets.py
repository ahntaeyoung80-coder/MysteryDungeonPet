"""Reuse retained original WAN sheets and ROM map parsers for the minimum gameplay pack."""
import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.mdpet.common import read_json, file_hash, write_json
from tools.mdpet.packer import pack_asset, write_package
from tools.compile_runtime_package import compile_package
from tools.package_format import encode_package

EXTRA = {'Attack': 1, 'Hurt': 6, 'Nod': 22, 'Faint': 33, 'HitGround': 34}


def prepare(root, output):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.graphics.chara_wan.handler import CharaWanHandler
    from tools.mdpet.extractor import checked_pack
    from tools.mdpet.wan_verify import verify_wan_asset
    if output.exists():
        raise ValueError('choose a new output directory')
    original = root / 'assets/original'
    rom_path = original / 'source/user-supplied.nds'
    before = file_hash(rom_path)
    rom = NintendoDSRom.fromFile(str(rom_path))
    packs = {p: checked_pack(rom.getFileByName(p)) for p in
             ('MONSTER/monster.bin', 'MONSTER/m_ground.bin', 'MONSTER/m_attack.bin')}
    with tempfile.TemporaryDirectory() as workspace:
        temp = Path(workspace)
        with zipfile.ZipFile(original / 'a1-original-package.zip') as archive:
            for name in archive.namelist():
                target = (temp / name).resolve()
                if not target.is_relative_to(temp.resolve()):
                    raise ValueError('unsafe archive path')
            archive.extractall(temp)
        source = temp / 'a1-rom-final-01'
        manifest = read_json(source / 'manifest.json')
        audit = temp / 'audit'; audit.mkdir()
        assets, reports = [], []
        original_checks = []
        for index, previous in enumerate(manifest['assets']):
            md = previous['source']['mdIndex']
            destination = audit / f'assets/{md}'; destination.mkdir(parents=True)
            sheets = source / f'sheets/{md}'
            asset, report = pack_asset(sheets, destination, previous['assetKey'], previous['source'],
                                      read_json(sheets / 'raw-wan.json'), EXTRA)
            available = {a['wanId'] for a in asset['animations']}
            if not set(EXTRA.values()).issubset(available):
                raise ValueError(f'missing gameplay WAN animations for MD {md}')
            wans = []
            for path, pack in packs.items():
                data = pack[previous['source']['spriteIndex']]
                if 'ground' not in path:
                    data = FileType.PKDPX.deserialize(data).decompress()
                wans.append(CharaWanHandler.deserialize(data))
            originals = deepcopy(wans)
            merged = CharaWanHandler.merge_wan(*wans)
            verification = verify_wan_asset(merged, asset, destination, originals)
            original_checks.append({'mdIndex': md, **verification})
            report['romPixelMatch'] = 'pass'
            asset['atlas']['file'] = f'assets/{md}/atlas.png'
            assets.append(asset); reports.append(report)
        write_package(audit, assets, reports)
        generated = temp / 'gameplay'; generated.mkdir()
        actor_report = compile_package(audit, generated / 'actors')
        backgrounds = FileType.BG_LIST_DAT.deserialize(rom.getFileByName('MAP_BG/bg_list.dat'))
        from PIL import Image
        atlas = Image.new('RGBA', (200, 400))
        maps = []
        for index, name in enumerate(['G01P06B', 'G01P07A']):
            entry = next(e for e in backgrounds.level if e.bma_name == name)
            refs = []
            def load(kind, filename, extension):
                path = f'MAP_BG/{filename.lower()}.{extension}'
                raw = rom.getFileByName(path)
                refs.append({'path': path, 'sha256': hashlib.sha256(raw).hexdigest()})
                return kind.deserialize(raw)
            bma = load(FileType.BMA, entry.bma_name, 'bma')
            bpc = load(FileType.BPC, entry.bpc_name, 'bpc')
            bpl = load(FileType.BPL, entry.bpl_name, 'bpl')
            bpas = [load(FileType.BPA, n, 'bpa') if n else None for n in entry.bpa_names]
            image = bma.to_pil(bpc, bpl, bpas, include_collision=False,
                               include_unknown_data_block=False, pal_ani=False, single_frame=True)[0].convert('RGBA')
            # The original dining actor is (220,184); sleep uses the map centre.
            center = (280, 212) if index == 0 else (image.width // 2, image.height // 2)
            left = max(0, min(image.width - 200, center[0] - 100))
            top = max(0, min(image.height - 200, center[1] - 100))
            crop = image.crop((left, top, left + 200, top + 200))
            crop.putalpha(255)  # Native map palette background is opaque; never expose the desktop.
            atlas.paste(crop, (0, 200 * index))
            maps.append({'name': name, 'sources': refs, 'sourceSize': image.size, 'crop': [left, top, 200, 200], 'animation': 'first original map frame; BPA/palette animation deferred'})
        scene = generated / 'home'; scene.mkdir()
        atlas.save(scene / 'atlas.png')
        encoded = {'assetVersion': 1, 'tickHz': 60, 'width': 200, 'height': 400,
                   'atlasName': 'atlas.png', 'pngBytes': (scene / 'atlas.png').stat().st_size,
                   'pngSha256': file_hash(scene / 'atlas.png'), 'assetKey': 'rom:EoS_NA:home',
                   'sourceKind': 1, 'provenance': json.dumps(maps), 'clips': []}
        for index, name in enumerate(['Dining', 'Bedroom']):
            encoded['clips'].append({'id': index, 'name': name, 'rows': [[{
                'rect': [0, index * 200, 200, 200], 'anchor': [0, 0], 'shadow': [0, 0], 'ticks': 1, 'markers': []}]]})
        (scene / 'home.mdpa').write_bytes(encode_package(encoded))
        (scene / 'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; user-provided original assets\n', encoding='utf-8')
        from tools.prepare_dungeon_props import prepare as prepare_props
        prepare_props(root,generated / "props")
        from tools.prepare_guild_scene import prepare as prepare_guild
        guild_report = prepare_guild(root, generated / 'guild')
        if file_hash(rom_path) != before:
            raise ValueError('ROM modified')
        report = {'romHashUnchanged': True, 'romSha256': before, 'extraWAN': EXTRA, 'originalWANChecks': original_checks, 'guildScene': guild_report,
                  'actors': actor_report, 'home': maps, 'originalWallTime': 'pending'}
        write_json(generated / 'provenance.json', report)
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(generated, output)
        return report


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    print(json.dumps(prepare(a.root, a.out), ensure_ascii=False, indent=2))
