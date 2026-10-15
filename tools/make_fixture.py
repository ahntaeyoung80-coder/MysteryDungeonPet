"""Generate a deterministic synthetic straight-RGBA PNG and MDPA fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib
from package_format import encode_package


def make_fixture():
    width, height = 48, 160
    pixels = bytearray(width * height * 4)
    for direction in range(8):
        for frame in range(3):
            for y in range(20):
                for x in range(16):
                    dx, dy = x - 8, y - 9
                    if dx * dx + dy * dy > 42:
                        continue
                    alpha = 128 if dx * dx + dy * dy > 30 else 255
                    rgba = (40 + direction * 22, 130 + frame * 35, 230, alpha)
                    if x == 6 + frame and y in (7, 8):
                        rgba = (0, 0, 0, alpha)  # Opaque black is preserved.
                    offset = ((direction * 20 + y) * width + frame * 16 + x) * 4
                    pixels[offset:offset+4] = bytes(rgba)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    scanlines = b''.join(b'\0' + pixels[y*width*4:(y+1)*width*4] for y in range(height))
    png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(scanlines, 9)) + chunk(b'IEND', b''))
    return png, encode_package(fixture_manifest(png))


def fixture_manifest(png):
    width, height = 48, 160
    def rows(directional, durations):
        return [[{'rect': [f*16, row*20, 16, 20], 'anchor': [8, 16], 'shadow': [8, 17],
                  'ticks': ticks, 'events': 1 << (f % 3), 'markers': [{'kind': 0, 'point': [8, 9]}, {'kind': 1, 'point': [8, 4]}]}
                 for f, ticks in enumerate(durations)] for row in range(8 if directional else 1)]
    clips = [{'id': n, 'name': name, 'rows': rows(directional, durations)} for n, name, directional, durations in (
        (0, 'Walk', True, [8, 10, 8]), (7, 'Idle', True, [60]), (15, 'Eat', False, [10, 10, 10]),
        (5, 'Sleep', False, [60, 60]), (14, 'Wake', True, [12, 12, 12]), (17, 'Pose', True, [20, 20, 20]))]
    clips.append({'id': 107, 'name': 'AliasIdle', 'aliasOf': 7, 'rows': []})
    manifest = {'assetVersion': 1, 'tickHz': 60, 'width': width, 'height': height, 'pngBytes': len(png),
                'sourceKind': 0, 'atlasName': 'atlas.png', 'pngSha256': hashlib.sha256(png).hexdigest(),
                'assetKey': 'test:synthetic:a2-v1',
                'provenance': 'Synthetic A-2 fixture; CC0-1.0; tools/make_fixture.py; no ROM identity; '
                              'sourceWanId=-1; clip IDs are runtime selectors only; tickHz=60 is test timing, '
                              'original timing unverified. 단일 행 동작과 8방향 계약 검사.',
                'clips': clips}
    return manifest


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets/fixture')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    png, packed = make_fixture()
    results = []
    for name, data in [('atlas.png', png), ('fixture.mdpa', packed)]:
        target = args.output / name
        if args.check:
            if target.read_bytes() != data:
                raise SystemExit(f'non-reproducible fixture: {target}')
        else:
            args.output.mkdir(parents=True, exist_ok=True)
            temporary = target.with_suffix(target.suffix + '.tmp')
            temporary.write_bytes(data)
            temporary.replace(target)
        results.append({'file': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    print(json.dumps({'fixture': 'synthetic, no ROM validation', 'files': results}, indent=2))


if __name__ == '__main__':
    main()
