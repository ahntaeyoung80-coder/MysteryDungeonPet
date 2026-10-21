"""Compare submitted body pixels using each restored NPC's original MD binding."""
import argparse
import csv
import json
from pathlib import Path
import subprocess
from PIL import Image


def check(inspector, root, output):
    results = []
    bodies = {str(c['mdIndex']): c['bodyManifest'] for c in json.loads(
        (root / 'assets/original/gameplay/npcs/catalogue.json').read_text(encoding='utf-8'))['characters']}
    scenarios = [
        ('follow', 'asset-1', 'companion_tile', 'trace'),
        ('attack', 'asset-3', None, 'trace'),
        ('friendly', 'asset-2', None, 'trace'),
        ('friendly', 'asset-2', None, 'restored-trace'),
        ('hostile', 'asset-3', None, 'trace'),
        ('hostile', 'asset-3', None, 'restored-trace'),
    ]
    for scenario, asset, tile_field, trace in scenarios:
        directory = output / scenario
        with (directory / f'{trace}.csv').open(newline='') as stream:
            row = next(r for r in csv.DictReader(stream) if r['submitted'] == '1')
        package_path = root / 'assets/original/gameplay/actors' / asset / 'asset.mdpa'
        if not tile_field:
            binding = next(s.split(':') for s in row['npc_bindings'].split(';') if s.startswith('1:'))
            package_path = root / 'assets/original/gameplay' / bodies[binding[1]]
            asset = f'md:{binding[1]}'
        package = json.loads(subprocess.check_output([str(inspector), str(package_path)], text=True))
        if tile_field:
            tile = int(row[tile_field])
            clip_id, frame_index, direction = int(row['companion_clip']), int(row['companion_frame']), 0
        else:
            actor = next(s.split(':') for s in row['actor_states'].split(';') if s.startswith('1:'))
            tile, clip_id, frame_index, direction = int(actor[1]), int(actor[2]), int(actor[3]), int(actor[5])
        clip = next(c for c in package['clips'] if c['id'] == clip_id)
        frame = clip['rows'][direction][frame_index]
        screen_x = tile % 24 * 24 + 12 - int(row['camera_x'])
        screen_y = tile // 24 * 24 + 12 - int(row['camera_y'])
        atlas = Image.open(package_path.parent / 'atlas.png').convert('RGBA')
        actual = Image.open(directory / f'{trace}-first.bmp').convert('RGB')
        x, y, width, height = frame['rect']
        ax, ay = frame['anchor']
        tested = matched = 0
        for yy in range(height):
            for xx in range(width):
                expected = atlas.getpixel((x + xx, y + yy))
                px, py = screen_x + xx - ax, screen_y + yy - ay
                if expected[3] != 255 or not (0 <= px < 200 and 0 <= py < 200):
                    continue
                tested += 1
                matched += actual.getpixel((px, py)) == expected[:3]
        if tested < 20 or matched / tested < 0.85:
            raise ValueError(f'{scenario}/{asset}/{trace}: original body pixels {matched}/{tested}')
        results.append({'scenario': scenario, 'asset': asset, 'trace': trace,
                        'matchedPixels': matched, 'testedPixels': tested, 'status': 'pass'})
    (output / 'sprite-pixels.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(results))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--inspector', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    check(args.inspector, Path(__file__).resolve().parents[1], args.output)
