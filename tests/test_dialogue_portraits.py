"""역할 선택을 바꿔도 실제 몸체 ID가 초상화/이름을 결정하는지 검사한다."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.prepare_dialogue_portraits import prepare

ROOT = Path(__file__).resolve().parents[1]
PORTRAITS = ROOT / 'assets/original/gameplay/portraits'


class DialogueIdentityTests(unittest.TestCase):
    def generate(self, config, catalogue=None):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            selection = folder / 'cast.json'
            selection.write_text(json.dumps(config), encoding='utf-8')
            mapping = None
            if catalogue is not None:
                mapping = folder / 'characters.json'
                mapping.write_text(json.dumps(catalogue), encoding='utf-8')
            result = prepare(ROOT, folder / 'output', selection, mapping)
            self.assertTrue(result['romUnchanged'])
            return result

    def config(self):
        return json.loads((PORTRAITS / 'cast.json').read_text(encoding='utf-8'))

    def test_body_selection_alone_swaps_character_portrait_and_name(self):
        config = self.config()
        player, partner = config['actors'][:2]
        player['bodyManifest'], partner['bodyManifest'] = partner['bodyManifest'], player['bodyManifest']
        actors = self.generate(config)['cast']
        self.assertEqual([(a['role'], a['portraitClip'], a['displayName']) for a in actors[:2]],
                         [(1, 4, '파이리'), (2, 1, '이상해씨')])
        self.assertEqual(actors[0]['bodyAssetKey'], 'rom:EoS_NA:md:4')
        self.assertTrue(actors[1]['characterId'].endswith(':EoS_NA:md:1'))

    def test_same_character_keeps_two_distinct_roles(self):
        config = self.config()
        config['actors'][1]['bodyManifest'] = config['actors'][0]['bodyManifest']
        result = self.generate(config)
        self.assertEqual(result['cast'][0]['characterId'], result['cast'][1]['characterId'])
        self.assertEqual([a['role'] for a in result['cast'][:2]], [1, 2])
        self.assertEqual(len(result['originalPortraits']), 3)

    def test_unknown_character_has_no_previous_portrait_fallback(self):
        catalogue = json.loads((PORTRAITS / 'characters.json').read_text(encoding='utf-8'))
        catalogue['characters'] = [c for c in catalogue['characters'] if c['mdIndex'] != 4]
        with self.assertRaisesRegex(ValueError, 'no verified portrait mapping for character MD4'):
            self.generate(self.config(), catalogue)

    def test_wrong_rom_catalogue_and_duplicate_roles_rejected(self):
        catalogue = json.loads((PORTRAITS / 'characters.json').read_text(encoding='utf-8'))
        catalogue['romSha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'catalogue ROM/schema mismatch'):
            self.generate(self.config(), catalogue)
        config = self.config()
        config['actors'][1]['role'] = 1
        with self.assertRaisesRegex(ValueError, 'exactly one'):
            self.generate(config)


if __name__ == '__main__':
    unittest.main()
