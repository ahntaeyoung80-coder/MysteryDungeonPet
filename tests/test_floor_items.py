import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from PIL import Image
from ndspy.rom import NintendoDSRom
from skytemple_files.common.types.file_types import FileType
from skytemple_files.common.util import get_ppmdu_config_for_rom
from tools.prepare_floor_items import prepare, selected_ids

ROOT = Path(__file__).resolve().parents[1]
ITEMS = ROOT/'assets/original/gameplay/items'
INSPECTOR = Path(os.environ.get('PET_PACKAGE_INSPECT', ROOT/'build/p0/pet_package_inspect'))


class FloorItemTests(unittest.TestCase):
    def test_every_item_id_uses_its_original_record_palette_and_pixels(self):
        audit = json.loads((ITEMS/'provenance.json').read_text())
        self.assertEqual([r['itemId'] for r in audit['items']], selected_ids(ROOT))
        self.assertEqual((len(audit['items']), audit['uniqueSprites']), (156, 39))
        rom = NintendoDSRom.fromFile(str(ROOT/'assets/original/source/user-supplied.nds'))
        records = FileType.ITEM_P.deserialize(rom.getFileByName('BALANCE/item_p.bin')).item_list
        pack = FileType.DUNGEON_BIN.deserialize(rom.getFileByName('DUNGEON/dungeon.bin'), get_ppmdu_config_for_rom(rom))
        atlas = Image.open(ITEMS/'atlas.png').convert('RGBA')
        for row in audit['items']:
            item = records[row['itemId']]
            self.assertEqual((row['sprite'], row['palette']), (item.sprite, item.palette))
            indexed = pack[1022].to_pil(item.sprite, item.palette)
            expected = indexed.convert('RGBA')
            expected.putalpha(Image.frombytes('L', expected.size, bytes(0 if x % 16 == 0 else 255 for x in indexed.tobytes())))
            x, y = row['cell'] % 16 * 16, row['cell'] // 16 * 16
            actual = atlas.crop((x, y, x+16, y+16))
            self.assertEqual(actual.tobytes(), expected.tobytes(), row['itemId'])
            self.assertEqual(hashlib.sha256(actual.tobytes()).hexdigest(), row['rgbaSha256'])
        if INSPECTOR.is_file():
            package = json.loads(subprocess.check_output([str(INSPECTOR), str(ITEMS/'items.mdpa')], text=True))
            self.assertEqual({c['id'] for c in package['clips']}, set(selected_ids(ROOT)))

    def test_selected_output_reproduces(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.assertTrue(prepare(ROOT, output)['romUnchanged'])
            for name in ('atlas.png', 'items.mdpa', 'provenance.json'):
                self.assertEqual((output/name).read_bytes(), (ITEMS/name).read_bytes())
