"""Extract a selected retained ROM Pokémon for dungeon NPC bodies."""
import argparse
from copy import deepcopy
import hashlib
from pathlib import Path
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.mdpet.common import file_hash, write_json
from tools.mdpet.extractor import checked_pack, raw_wan_metadata
from tools.mdpet.packer import pack_asset, write_package
from tools.mdpet.wan_verify import verify_wan_asset
from tools.compile_runtime_package import compile_package


def prepare(root, output, index, scene_actor=False):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.util import get_ppmdu_config_for_rom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.graphics.chara_wan.handler import CharaWanHandler
    if output.exists():
        raise ValueError('choose a new output directory')
    rom_path = root / 'assets/original/source/user-supplied.nds'
    before = file_hash(rom_path)
    rom = NintendoDSRom.fromFile(str(rom_path)); config = get_ppmdu_config_for_rom(rom)
    md = FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md'))
    if not 0 <= index < len(md.entries):
        raise ValueError('MD index outside retained table')
    record = md.entries[index]; dex = record.national_pokedex_number
    if dex <= 0 or record.sprite_index < 0:
        raise ValueError('select an ordinary Pokémon with a sprite')
    refs, wans = [], []
    for path in ('MONSTER/monster.bin', 'MONSTER/m_ground.bin', 'MONSTER/m_attack.bin'):
        data = checked_pack(rom.getFileByName(path))[record.sprite_index]
        refs.append({'sourcePath': path, 'packSlot': record.sprite_index, 'contentHash': hashlib.sha256(data).hexdigest()})
        if 'ground' not in path:
            data = FileType.PKDPX.deserialize(data).decompress()
        wans.append(CharaWanHandler.deserialize(data))
    originals = deepcopy(wans); merged = CharaWanHandler.merge_wan(*wans)
    with tempfile.TemporaryDirectory() as workspace:
        temp = Path(workspace); sheets = temp / 'sheets'; sheets.mkdir()
        CharaWanHandler.export_sheets(str(sheets), merged, config.animation_names.get(record.sprite_index, config.animation_names[0]))
        names = {int(n.findtext('Index')): n.findtext('Name') for n in ET.parse(sheets/'AnimData.xml').findall('Anims/Anim') if n.find('Index') is not None}
        required = {names[i]: i for i in ((0,1,5,6,7,13,14,15,17,22,27,33,34) if scene_actor else (0,1,6,7))}
        optional = {names[i]:i for i in (22,33) if i in names}
        source = {'sourceKind':'rom', 'romHash': before, 'edition':config.game_edition,
                  'mdIndex':index, 'spriteIndex':record.sprite_index, 'nationalDex':dex, 
                  'toolCommit':'5ed359a2560a1f04666b9ea02b71649b89cb16d7', 'sourceRefs':refs,
                  'license':'game_assets_user_local_only', 'credit':'Nintendo / Pokémon / CHUNSOFT'}
        audit = temp/'audit'; audit.mkdir()
        asset, report = pack_asset(sheets, audit, f'rom:{config.game_edition}:monster:{index}', source,
                                   raw_wan_metadata(merged), required_animations=required, optional_animations=optional)
        verification = verify_wan_asset(merged, asset, audit, originals); report['romPixelMatch']='pass'
        write_package(audit, [asset], [report]); compile_package(audit, temp/'compiled')
        shutil.copytree(temp/'compiled/asset-0', output)
        write_json(output/'provenance.json', {'source':source, 'verification':verification, 'missingSceneClips':{str(i):fallback for i,fallback in ((22,'finite Idle7 for NPC response'),(33,'finite Hurt6 before removal')) if i not in names}, 'selection':'explicit MD record from retained ROM'})
    if file_hash(rom_path) != before:
        raise ValueError('ROM changed')
    return {'mdIndex':index, 'spriteIndex':record.sprite_index, 'nationalDex':dex, **verification}

if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1]); p.add_argument('--out', type=Path, required=True); p.add_argument('--md-index',type=int,required=True)
    args = p.parse_args(); print(prepare(args.root, args.out, args.md_index))
