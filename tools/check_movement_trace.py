"""Check production Windows frame traces against movement and viewport contracts."""
import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check_trace(path, kind, scale):
    with Path(path).open(encoding='utf-8-sig', newline='') as stream:
        rows = list(csv.DictReader(stream))
    require(len(rows) >= 3, 'missing native composed-frame evidence')
    groups = defaultdict(list)
    pixels = {}
    viewport = (200 + scale - 1) // scale
    anchor, maximum = viewport // 2, 576 - viewport
    previous_time = -1
    submitted, directions = 0, set()
    walking_start = None
    for row in rows:
        stamp = int(row['elapsed_us'])
        require(stamp >= previous_time, 'non-monotonic frame trace')
        previous_time = stamp
        require(int(row['scale']) == scale, 'unexpected scale')
        require(int(row['life']) == 0 and int(row['scene_beat']) == 0, 'movement obscured by interaction/fade')
        require(int(row['opaque_pixels']) == 40000, 'transparent dungeon viewport')
        world = [float(row[f'world_{axis}']) for axis in ('x', 'y')]
        scene = [int(row[f'scene_{axis}']) for axis in ('x', 'y')]
        camera = [int(row[f'camera_{axis}']) for axis in ('x', 'y')]
        require(scene == [math.floor(p + 0.5) for p in world], 'actor raster rounding differs')
        require(camera == [min(max(p - anchor, 0), maximum) for p in scene], 'camera/actor origin mismatch')
        submitted += int(row['submitted'])
        if kind == 'edge':
            require(int(row['moving']) == 0, 'stationary edge fixture moved')
            key = (row['clip'], row['direction'], row['frame_index'])
            value = (row['surface_hash'], *scene, *camera)
            require(key not in pixels or pixels[key] == value, 'same stationary frame changed pixels/position')
            pixels[key] = value
            continue
        action = int(row['action_id'])
        if not action:
            continue
        if walking_start is None:
            walking_start = row['walk_started_ms']
        require(row['walk_started_ms'] == walking_start, 'successive steps reset saved walking phase')
        require(int(row['duration_ms']) == 400 and int(row['clip']) == 0, 'moving step is not 400ms Walk')
        start, end = int(row['from_tile']), int(row['to_tile'])
        dx, dy = end % 24 - start % 24, end // 24 - start // 24
        directions.add(int(row['direction']))
        expected_direction = [(0, 1), (1, 1), (1, 0), (1, -1), (0, -1), (-1, -1), (-1, 0), (-1, 1)].index((dx, dy))
        require(int(row['direction']) == expected_direction, 'body faces away from movement')
        fractions = []
        for axis, delta, coordinate in [(0, dx, start % 24), (1, dy, start // 24)]:
            origin = coordinate * 24 + 12
            if delta:
                fractions.append((world[axis] - origin) / (24 * delta))
            else:
                require(abs(world[axis] - origin) < 1e-6, 'movement wandered off path')
        require(all(-1e-6 <= f <= 1 + 1e-6 for f in fractions), 'movement overshot tile')
        require(max(fractions) - min(fractions) < 1e-6, 'diagonal axes use different clocks')
        groups[action].append((row, fractions[0]))
    if kind != 'edge':
        require(len(groups) >= 4, 'too few consecutive steps observed')
        complete = list(groups.values())[:-1]
        for previous, current in zip(complete, list(groups.values())[1:]):
            require(previous[0][0]['to_tile'] == current[0][0]['from_tile'], 'step endpoint disconnected')
            delta = int(current[0][0]['started_ms']) - int(previous[0][0]['started_ms'])
            require(399 <= delta <= 550, f'continuous movement inserted a wait: {delta}ms')
        for samples in complete:
            mid = {r['scene_x'] + ',' + r['scene_y'] for r, t in samples if 0.05 < t < 0.95}
            require(len(mid) >= 4, 'missing intermediate composed positions')
        require(submitted >= 12, 'intermediate frames were not submitted to Windows')
        if kind == 'turn':
            require(len(directions) >= 2, 'turn scenario did not observe direction change')
    return {'status': 'pass', 'kind': kind, 'scale': scale, 'frames': len(rows),
            'submittedFrames': submitted, 'actions': len(groups), 'directions': sorted(directions),
            'opaquePixels': 40000, 'nativeWindows11': 'not observed'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--kind', choices=['straight', 'turn', 'edge'], required=True)
    parser.add_argument('--scale', type=int, choices=[1, 2, 3], required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = check_trace(args.trace, args.kind, args.scale)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
