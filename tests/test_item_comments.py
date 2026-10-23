import copy
import json
from pathlib import Path
import unittest
from tools.compile_item_comments import compile_comments
from tools.compile_dialogue_catalogue import pages
ROOT=Path(__file__).resolve().parents[1]
class ItemCommentsTests(unittest.TestCase):
    def setUp(self):
        self.source=json.loads((ROOT/'assets/original/gameplay/dialogue/item_comments.json').read_text())
    def test_reproducible_all_selected_items_have_three_short_choices(self):
        self.assertEqual(compile_comments(self.source),(ROOT/'src/core/item_comments.inc').read_text())
        self.assertEqual(len(self.source['items']),156)
        for row in self.source['items']:
            self.assertEqual(len(set(row['lines'])),3)
            for text in row['lines']:self.assertEqual(len(pages(text)),1)
    def test_rejects_duplicate_choices_and_overlong_comments(self):
        for change in ('duplicate','long'):
            source=copy.deepcopy(self.source)
            source['items'][0]['lines'][1]=source['items'][0]['lines'][0] if change=='duplicate' else '아'*100
            with self.assertRaises(ValueError):compile_comments(source)
