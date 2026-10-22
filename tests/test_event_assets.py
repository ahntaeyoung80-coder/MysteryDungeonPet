"""자동 후보인 새 맵·사건 문맥·종결 장소를 보유 원작에서 다시 대조한다."""
from pathlib import Path
import tempfile
import json
import unittest
from tools.mdpet.b1 import build
from tools.prepare_event_contexts import prepare as prepare_contexts
from tools.prepare_completion_scenes import prepare as prepare_completion

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / 'assets/original/gameplay'


class EventAssetsTests(unittest.TestCase):
    def test_selected_map18_preserves_original_profiles_and_tiles(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / 'map'
            report = build(ROOT / 'assets/original/source/user-supplied.nds',
                           ROOT / 'assets/original/a1-original-package.zip', out, 18)
            self.assertEqual(report['floorCount'], 9)
            self.assertEqual(report['romHashBefore'], report['romHashAfter'])
            selected = ROOT / 'assets/original/exploration/dungeon-18'
            for file in selected.iterdir():
                if file.suffix in ('.mdpe', '.mdpa', '.png'):
                    self.assertEqual(file.read_bytes(), (out / file.name).read_bytes(), file.name)

    def test_original_special_context_weights_reproduce(self):
        from ndspy.rom import NintendoDSRom
        from skytemple_files.common.types.file_types import FileType
        from skytemple_files.common.util import get_ppmdu_config_for_rom,get_binary_from_rom
        from skytemple_files.hardcoded.dungeons import HardcodedDungeons
        from tools.mdpet.b1 import load_catalogue
        from tools.mdpet.catalogue import build_floor
        rom=NintendoDSRom.fromFile(str(ROOT/'assets/original/source/user-supplied.nds'))
        config=get_ppmdu_config_for_rom(rom)
        definitions=HardcodedDungeons.get_dungeon_list(get_binary_from_rom(rom,config.bin_sections.arm9),config)
        mappa=FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'))
        a=load_catalogue(ROOT/'assets/original/a1-original-package.zip')
        retained=json.loads((GAME/'events/provenance.json').read_text())['floors']
        self.assertEqual(len(retained),160)
        for p in retained:
            key=p['key'];dungeon=key['dungeonId']
            actual=build_floor(config.game_edition,dungeon,key['localFloor'],definitions[dungeon],mappa.floor_lists,
                               a['pokemon'],a['items'],{int(k):v for k,v in a['itemCategoryMembers'].items()})
            for field in ('key','tilesetId','groupFloor','rawLayout','monsters','itemContexts','traps'):
                self.assertEqual(actual[field],p[field],(dungeon,key['localFloor'],field))

    def test_verified18_to19_completion_reproduces_and_keeps15(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / 'completion'
            prepare_completion(ROOT, out, 18, GAME / 'completion/destinations.mdec')
            for name in ('amp-clearing.mdpa', 'amp-clearing.png', 'destinations.mdec'):
                self.assertEqual((out / name).read_bytes(), (GAME / 'completion' / name).read_bytes(), name)
