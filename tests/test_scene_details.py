"""원작 효과/대화창 추출 결과가 ROM 픽셀과 메타데이터를 보존하는지 확인."""
import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image
from tools.prepare_scene_details import prepare

ROOT = Path(__file__).resolve().parents[1]

class SceneDetailsTests(unittest.TestCase):
    def test_original_effect_and_dialogue_tiles_reproduce(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)/'export'
            result = prepare(ROOT, output)
            self.assertEqual((result['appleFrames'],result['totalTicks']), (13,52))
            self.assertTrue(result['romUnchanged'])
            for source, target in [('effects','effects'),('ui','ui')]:
                retained=ROOT/'assets/original/gameplay'/target
                for path in (output/source).iterdir():
                    self.assertEqual(path.read_bytes(),(retained/path.name).read_bytes(),path.name)
        apple=json.loads((ROOT/'assets/original/gameplay/effects/provenance.json').read_text())
        self.assertGreater(len({tuple(f['offset']) for f in apple['frames']}),6)
        self.assertGreater(len({f['rgbaSha256'] for f in apple['frames']}),3)
        self.assertEqual(apple['entryHex'],'0200000001000000010000000f000000ffffffff0000000000000001')
        atlas=Image.open(ROOT/'assets/original/gameplay/ui/atlas.png')
        self.assertEqual(atlas.size,(144,8))
        self.assertEqual(atlas.mode,'RGBA')
