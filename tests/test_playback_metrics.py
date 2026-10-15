import copy
import csv
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.check_playback_metrics import check_metrics, PLAYBACK_COLUMNS


class PlaybackMetricsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        inspector = os.environ.get('PET_PACKAGE_INSPECT')
        if not inspector:
            raise unittest.SkipTest('set PET_PACKAGE_INSPECT for native package/log consistency tests')
        cls.package = json.loads(subprocess.check_output([inspector, 'assets/fixture/fixture.mdpa'], text=True))

    def row(self, clip_id=0, direction=5, index=1, elapsed=1, final=0):
        clip = next(c for c in self.package['clips'] if c['id'] == clip_id)
        resolved = 0 if len(clip['rows']) == 1 else direction
        frame = clip['rows'][resolved][index]
        values = (clip_id, direction, resolved, index, 1, 33, 33, frame['ticks'], frame['rawFlag'],
                  *frame['anchor'], *frame['rect'])
        row = dict(zip(PLAYBACK_COLUMNS, values))
        row.update(elapsed_seconds=elapsed, final_sample=final, dpi=144, physical_size=200,
                   integer_scale=3, window_x=-500, window_y=100, marker_count=1, finite_finish_count=0,
                   skipped_cycles=0, render_submissions=1, submissions_while_paused=0)
        return row

    def check_rows(self, rows, scenario='manual'):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'metrics.csv'
            with path.open('w', encoding='utf-8', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
            return check_metrics(path, self.package, scenario)

    def test_native_metadata_alias_and_one_row_and_final_live_window(self):
        rows = [self.row(), self.row(107, 4, 0, elapsed=2), self.row(15, 7, 0, elapsed=3, final=1)]
        result = self.check_rows(rows)
        self.assertEqual(result['status'], 'pass')
        self.assertEqual(result['observedSelectors'], [(0, 5), (15, 7), (107, 4)])
        self.assertEqual(result['finalWindow']['dpi'], 144)
        self.assertEqual(result['finalWindow']['window_x'], -500)
        self.assertEqual(result['sourceKind'], 0)  # Test fixture, not ROM validation.

    def test_final_sample_and_destroyed_window_regressions_are_rejected(self):
        rows = [self.row(), self.row(elapsed=2, final=1)]
        for field, value in [('dpi', 0), ('physical_size', 100), ('final_sample', 0)]:
            broken = copy.deepcopy(rows)
            broken[-1][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.check_rows(broken)
        broken = copy.deepcopy(rows)
        broken[0]['final_sample'] = 1
        with self.assertRaises(ValueError):
            self.check_rows(broken)

    def test_mismatched_direction_frame_anchor_and_timing_are_rejected(self):
        for field, value in [('resolved_direction', 0), ('frame_index', 99), ('frame_ticks', 999),
                             ('frame_flag', 0), ('anchor_x', -200), ('rect_y', 0), ('scene_x', 34),
                             ('animation_id', 999), ('submissions_while_paused', 1)]:
            row = self.row(final=1)
            row[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.check_rows([row])

    def test_empty_baseline_and_contaminated_animation(self):
        row = self.row(final=1)
        for field in PLAYBACK_COLUMNS:
            row[field] = -1 if field in PLAYBACK_COLUMNS[:4] else 0
        row['frame_flag'] = 0xffffffff
        row['marker_count'] = 0
        self.assertEqual(self.check_rows([row], 'empty')['status'], 'pass')
        for field in ('animation_active', 'marker_count', 'finite_finish_count', 'skipped_cycles', 'rect_w'):
            broken = dict(row, **{field: 1})
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.check_rows([broken], 'empty')

    def test_historical_columns_nan_and_nonincreasing_clock_rejected(self):
        row = self.row(final=1)
        del row['animation_id']
        with self.assertRaisesRegex(ValueError, 'historical A-2'):
            self.check_rows([row])
        with self.assertRaises(ValueError):
            self.check_rows([self.row(), self.row(elapsed=1, final=1)])
        row = self.row(final=1)
        row['window_x'] = float('nan')
        with self.assertRaises(ValueError):
            self.check_rows([row])


if __name__ == '__main__':
    unittest.main()
