"""A-3의 새 CSV와 native package inspector를 대조한다. OS/원작 시간 판정은 별도다."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess


PLAYBACK_COLUMNS = ('animation_id', 'direction', 'resolved_direction', 'frame_index',
                    'animation_active', 'scene_x', 'scene_y', 'frame_ticks', 'frame_flag',
                    'anchor_x', 'anchor_y', 'rect_x', 'rect_y', 'rect_w', 'rect_h')


def check_metrics(path, package, scenario):
    with path.open(encoding='utf-8-sig', newline='') as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or len(reader.fieldnames) != len(set(reader.fieldnames)):
            raise ValueError('missing/duplicate CSV columns')
        required = set(PLAYBACK_COLUMNS) | {'elapsed_seconds', 'final_sample', 'dpi', 'window_x', 'window_y',
                    'physical_size', 'integer_scale', 'marker_count', 'finite_finish_count', 'skipped_cycles',
                    'render_submissions', 'submissions_while_paused'}
        if not required.issubset(reader.fieldnames):
            raise ValueError('A-3 observability columns missing; historical A-2 records are not new verification')
        rows = []
        for raw in reader:
            if None in raw or any(value is None for value in raw.values()):
                raise ValueError('malformed CSV row')
            row = {key: float(value) for key, value in raw.items()}
            if not all(math.isfinite(value) for value in row.values()):
                raise ValueError('nonfinite CSV value')
            if any(row[key] != int(row[key]) for key in required - {'elapsed_seconds'}):
                raise ValueError('fractional playback/window/count field')
            rows.append(row)
    if not rows:
        raise ValueError('no samples')
    clips = {clip['id']: clip for clip in package['clips']}
    selectors = set()
    previous = -1
    for index, row in enumerate(rows):
        def require(condition, reason):
            if not condition:
                raise ValueError(f'row {index + 1}: {reason}')
        require(row['elapsed_seconds'] > previous, 'non-increasing timestamp')
        previous = row['elapsed_seconds']
        require(row['final_sample'] == (index == len(rows) - 1), 'final sample missing/duplicated/not last')
        require(row['dpi'] > 0 and row['physical_size'] == 200 and 1 <= row['integer_scale'] <= 8,
                'invalid live window DPI/physical size/scale')
        require(row['submissions_while_paused'] == 0, 'submission while paused')
        require(row['animation_active'] in (0, 1), 'invalid active flag')
        if scenario == 'empty':
            require(all(row[key] == -1 for key in PLAYBACK_COLUMNS[:4]) and row['animation_active'] == 0,
                    'empty baseline still has playback')
            require(all(row[key] == 0 for key in ('scene_x', 'scene_y', 'frame_ticks', 'rect_w', 'rect_h',
                                                 'marker_count', 'finite_finish_count', 'skipped_cycles')),
                    'empty baseline animation counters/geometry changed')
            require(row['render_submissions'] <= 1, 'empty baseline resubmitted')
            continue
        clip = clips.get(row['animation_id'])
        require(clip is not None, 'unknown recorded animation ID')
        direction = int(row['direction'])
        require(0 <= direction < 8, 'invalid direction')
        resolved = 0 if len(clip['rows']) == 1 else direction
        require(row['resolved_direction'] == resolved, 'one/eight-row resolver mismatch')
        frames = clip['rows'][resolved]
        frame_index = int(row['frame_index'])
        require(0 <= frame_index < len(frames), 'frame out of bounds')
        frame = frames[frame_index]
        expected = (frame['ticks'], frame['rawFlag'], *frame['anchor'], *frame['rect'])
        actual = tuple(row[key] for key in PLAYBACK_COLUMNS[7:])
        require(actual == expected, 'frame/tick/flag/anchor/rect mismatch with native package')
        scene = 200 // (2 * int(row['integer_scale']))
        require(row['scene_x'] == scene and row['scene_y'] == scene, 'scene position mismatch with fixed viewport')
        selectors.add((int(row['animation_id']), direction))
    return {'status': 'pass', 'sampleCount': len(rows), 'sourceKind': package['sourceKind'],
            'csvSHA256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'observedSelectors': sorted(selectors),
            'finalWindow': {key: rows[-1][key] for key in ('dpi', 'window_x', 'window_y', 'physical_size')},
            'scope': 'CSV metadata/native package consistency only; OS events, temporal playback and original timing pending'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--inspector', type=Path, required=True)
    parser.add_argument('--scenario', choices=('manual', 'empty', 'idle', 'walk', 'hide', 'switch'), default='manual')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    package = json.loads(subprocess.check_output([str(args.inspector.resolve()), str(args.package)], text=True, timeout=30))
    result = check_metrics(args.csv, package, args.scenario)
    result['packageSHA256'] = hashlib.sha256(args.package.read_bytes()).hexdigest()
    text = json.dumps(result, indent=2, allow_nan=False) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    print(text, end='')


if __name__ == '__main__':
    main()
