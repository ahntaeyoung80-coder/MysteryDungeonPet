import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

from tools.mdpet.b1 import build, load_catalogue, write_profiles

ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT / 'assets/original/source/user-supplied.nds'
ZIP = ROOT / 'assets/original/a1-original-package.zip'
NATIVE = Path(os.environ.get('PET_EXPLORE', ROOT / 'build/b1/pet_explore'))


class B1BridgeTests(unittest.TestCase):
    def test_original_conversion_reproducible_and_closed(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as d:
            a, b = Path(d) / 'a', Path(d) / 'b'
            report = build(ROM, ZIP, a, 0)
            second = build(ROM, ZIP, b, 0)
            self.assertEqual(report, second)
            for p in a.iterdir():
                self.assertEqual(p.read_bytes(), (b / p.name).read_bytes())
            self.assertEqual(report['romHashBefore'], report['romHashAfter'])
            self.assertEqual(report['floorCount'], 3)
            self.assertEqual(report['stairsGraphic'], 'unresolved; logical marker only')
            self.assertEqual((a / 'exploration.mdpe').read_bytes(),
                             (ROOT / 'assets/original/exploration/dungeon-0/exploration.mdpe').read_bytes())
            atlas = Image.open(a / 'tileset-0.png').convert('RGBA')
            self.assertEqual(atlas.size, (480, 480))
            with self.assertRaises(FileExistsError):
                build(ROM, ZIP, a, 0)

    def test_normal_and_floor_items_only(self):
        catalogue = load_catalogue(ZIP)
        for floor in catalogue['floors']:
            data = write_profiles([floor])
            changed = json.loads(json.dumps(floor))
            changed['monsters']['monsterHouse']['entries'] = []
            for c in changed['itemContexts']:
                if c['context'] != 'floor_items':
                    c['conditionalItems'] = []
            self.assertEqual(data, write_profiles([changed]))
            for row in floor['monsters']['normal']['entries']:
                self.assertNotIn(row['originalId'], (0, 0x229, 383, 983))
        floor = catalogue['floors'][0]
        floor['itemContexts'][0]['categories']['guaranteed'] = [99]
        with self.assertRaisesRegex(ValueError, 'guaranteed category'):
            write_profiles([floor])

    @unittest.skipUnless(NATIVE.is_file(), 'native explorer unavailable')
    def test_native_autotile_matches_skytemple_drawer(self):
        from tools.mdpet.extractor import smoke
        from ndspy.rom import NintendoDSRom
        from skytemple_files.common.util import get_ppmdu_config_for_rom
        from skytemple_files.common.types.file_types import FileType
        from skytemple_files.graphics.dma.dma_drawer import DmaDrawer
        from skytemple_files.graphics.dma.protocol import DmaType
        smoke()
        rom = NintendoDSRom.fromFile(str(ROM))
        config = get_ppmdu_config_for_rom(rom)
        pack = FileType.DUNGEON_BIN.deserialize(rom.getFileByName('DUNGEON/dungeon.bin'), static_data=config)
        for dungeon, tileset in ((0, 0), (15, 12)):
            package = ROOT / f'assets/original/exploration/dungeon-{dungeon}/exploration.mdpe'
            drawer = DmaDrawer(pack[170 + tileset])
            for seed in range(1, 101):
                data = json.loads(subprocess.check_output([str(NATIVE), str(package), str(seed), '--grid']))
                terrain = data['terrain']
                rules = [[DmaType.FLOOR if terrain[y * 24 + x] else DmaType.WALL for x in range(24)] for y in range(24)]
                expected = drawer.get_mappings_for_rules(rules, variation_index=0, treat_outside_as_wall=True)
                self.assertEqual(data['chunks'], [chunk for row in expected for chunk in row])

    @unittest.skipUnless(NATIVE.is_file(), 'native explorer unavailable')
    def test_durable_home_does_not_replay_pickup_or_clear(self):
        package = ROOT / 'assets/original/exploration/dungeon-0/exploration.mdpe'
        with tempfile.TemporaryDirectory() as d:
            snapshot = Path(d) / 'run.mdes'
            run = json.loads(subprocess.check_output([str(NATIVE), str(package), '55', str(snapshot)]))
            resumed = json.loads(subprocess.check_output([str(NATIVE), str(package), '99', str(snapshot)]))
            self.assertTrue(run['completed'])
            self.assertEqual(run['newClears'], 1)
            self.assertEqual(resumed['newClears'], 0)
            self.assertEqual(resumed['newPickups'], 0)
            self.assertEqual(run['inventory'], resumed['inventory'])
            self.assertEqual(run['rngState'], resumed['rngState'])

    @unittest.skipUnless(NATIVE.is_file(), 'native explorer unavailable')
    def test_process_kill_preserves_committed_run(self):
        package = ROOT / 'assets/original/exploration/dungeon-15/exploration.mdpe'
        with tempfile.TemporaryDirectory() as d:
            snapshot = Path(d) / 'killed.mdes'
            process = subprocess.Popen([str(NATIVE), str(package), '71', str(snapshot)],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            deadline = time.monotonic() + 10
            try:
                while not snapshot.exists() and process.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.001)
                self.assertTrue(snapshot.exists(), 'no durable checkpoint before kill')
                self.assertIsNone(process.poll(), 'process finished before kill test')
                process.kill()
                process.communicate(timeout=10)
            finally:
                if process.poll() is None: process.kill(); process.communicate()
            resumed = json.loads(subprocess.check_output([str(NATIVE), str(package), '71', str(snapshot)]))
            baseline = Path(d) / 'baseline.mdes'
            expected = json.loads(subprocess.check_output([str(NATIVE), str(package), '71', str(baseline)]))
            self.assertTrue(resumed['completed'])
            self.assertEqual(resumed['inventory'], expected['inventory'])
            self.assertEqual(resumed['rngState'], expected['rngState'])
            self.assertEqual(snapshot.read_bytes(), baseline.read_bytes())


if __name__ == '__main__':
    unittest.main()
