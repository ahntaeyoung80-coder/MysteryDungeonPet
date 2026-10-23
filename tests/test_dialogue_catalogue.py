import copy
import json
from pathlib import Path
import unittest
from tools.compile_dialogue_catalogue import compile_catalogue, pages

ROOT = Path(__file__).resolve().parents[1]


class DialogueCatalogueTests(unittest.TestCase):
    def setUp(self):
        self.source = json.loads((ROOT / 'assets/original/gameplay/dialogue/catalogue.json').read_text())

    def test_compilation_is_reproducible(self):
        self.assertEqual(compile_catalogue(self.source), (ROOT / 'assets/original/gameplay/dialogue/catalogue.mdlc').read_bytes())
        self.assertEqual(sum(not b['context'].startswith('event.') and b['context']!='boss.befriend' for b in self.source['bundles']), 56)

    def test_long_korean_pages_fit_two_lines_and_keep_text(self):
        text = '가나다라마바사아자차카타파하' * 10
        result = pages(text)
        self.assertGreater(len(result), 1)
        self.assertEqual(''.join(p.replace('\n', '') for p in result), text)
        for page in result:
            self.assertLessEqual(len(page.split('\n')), 2)
            for row in page.split('\n'):
                self.assertLessEqual(sum(1 if ord(c) < 128 else 2 for c in row), 26)

    def test_mornings_are_bright_partner_lines_and_player_is_quiet(self):
        for bundle in self.source['bundles']:
            for line in bundle['lines']:
                self.assertNotEqual(line['speaker'], 1, bundle['id'])
                if bundle['context'] in ('wake.alarm', 'morning.depart'):
                    self.assertEqual((line['speaker'], line['listener'], line['emotion']), (4 if bundle['context']=='wake.alarm' else 2, 1, 1))
        for context in ('companion.checkin', 'npc.greet', 'sleep.greet', 'wake.alarm', 'morning.depart'):
            choices = [b for b in self.source['bundles'] if b['context'] == context]
            self.assertGreaterEqual(len(choices), 8)
            self.assertEqual(len({b['lines'][0]['text'] for b in choices}), len(choices))

    def test_page_boundary_keeps_short_korean_words_together(self):
        text = '서두를 것 없어. 천천히 둘러봐!'
        result = pages(text)
        self.assertEqual(' '.join(p.replace('\n', ' ') for p in result), text)
        self.assertTrue(any('둘러봐!' in page for page in result))

    def test_short_sentence_stays_on_one_two_row_page(self):
        result = pages('좋은 아침이다아! 다들 일어날 시간이다아!')
        self.assertEqual(len(result), 1)
        self.assertIn('\n', result[0])

    def test_invalid_conditions_actors_ids_and_emotions_are_rejected(self):
        for field, value in [('context', 'not.implemented'), ('condition', 'future.success'), ('style', 'official.personality'), ('weight', 0)]:
            bad = copy.deepcopy(self.source); bad['bundles'][0][field] = value
            with self.assertRaises(ValueError): compile_catalogue(bad)
        for field, value in [('speaker', 4), ('emotion', 14), ('holdMs', 0), ('text', '')]:
            bad = copy.deepcopy(self.source); bad['bundles'][0]['lines'][0][field] = value
            with self.assertRaises(ValueError): compile_catalogue(bad)
        bad = copy.deepcopy(self.source); bad['bundles'].append(bad['bundles'][0])
        with self.assertRaises(ValueError): compile_catalogue(bad)
