"""Compile validated A-1 audit manifests into the MDPA runtime representation.

One logical asset contract: JSON is offline evidence; MDPA contains runtime fields
and opaque source metadata. Never extracts a ROM or silently changes timing.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.mdpet.common import file_hash, read_json, safe_file, staging, write_json, integer
from tools.mdpet.packer import validate_package
from tools.package_format import encode_package

POINTS = ('center', 'head', 'lhand', 'rhand', 'shadow')
EVENTS = ('rush', 'hit', 'return')


def canonical_text(data):
    return json.dumps(data, ensure_ascii=False, sort_keys=True, separators=(',', ':'), allow_nan=False)


def convert_asset(asset, manifest_hash):
    atlas = asset['atlas']
    if atlas['decodedBytes'] > 8 * 1024 * 1024 or atlas['byteSize'] > 8 * 1024 * 1024:
        raise ValueError('A-2 single-atlas 8MiB budget exceeded; split/select offline, do not hide missing assets')
    clips = asset['animations']
    by_name = {clip['name']: clip for clip in clips}
    # Original WAN IDs are selectors; unnamed optional aliases get an explicit local ID.
    ids = {}
    used = set()
    for clip in clips:
        selector = clip.get('wanId')
        if selector is not None and selector >= 0:
            integer(selector, 'wanId', 0, 0x7fffffff)
            if selector in used:
                raise ValueError('duplicate WAN selector')
            ids[clip['name']] = selector
            used.add(selector)
    candidate_id = 10000
    for clip in clips:
        if clip['name'] not in ids:
            while candidate_id in used:
                candidate_id += 1
            ids[clip['name']] = candidate_id
            used.add(candidate_id)
    packed_clips = []
    provisional = set()
    for clip in clips:
        target = clip
        while target['aliasOf'] is not None:
            target = by_name[target['aliasOf']]
        timing = clip['sourceTiming']
        hz = timing.get('tickHz') or timing.get('provisionalTickHz')
        integer(hz, 'tickHz/provisionalTickHz', 1, 1000)
        provisional.add(hz)
        if timing.get('rawSpeed') is not None:
            raise ValueError('rawSpeed semantics need an explicit adapter; not guessed')
        directions = copy.deepcopy(target['directions'])
        raw_directions = clip.get('rawDirections')
        if raw_directions is not None:
            if len(raw_directions) != len(directions):
                raise ValueError('alias raw direction count differs')
            for row, raw_row in enumerate(raw_directions):
                if not 1 <= len(raw_row) <= len(directions[row]):
                    raise ValueError('alias raw frame count differs')
                directions[row] = directions[row][:len(raw_row)]
                for frame, raw in zip(directions[row], raw_row):
                    previous = frame.get('rawWan')
                    if previous and any(raw[k] != previous[k] for k in ('offset', 'shadow')):
                        raise ValueError('alias geometry differs; materialize correct frames in A-1 instead of guessing')
                    frame['durationTicks'] = raw['durationTicks']
                    frame['markers'] = raw['markers']
                    frame['rawWan'] = raw
        rows = []
        for direction in directions:
            frames = []
            for frame in direction:
                raw = frame.get('rawWan')
                ticks = integer(frame['durationTicks'], 'durationTicks', 1, 65535)
                events = 0
                for event in frame['markers']:
                    if event not in EVENTS:
                        raise ValueError('unknown frame event')
                    events |= 1 << EVENTS.index(event)
                markers = [{'kind': POINTS.index(name), 'point': point} for name, point in frame['points'].items() if point is not None]
                frames.append({'rect': frame['rect'], 'anchor': frame['anchor'], 'shadow': frame['points'].get('shadow') or [0, 0],
                               'ticks': ticks, 'markers': markers, 'shadowRect': frame['shadowRect'], 'cropOrigin': frame['cropOrigin'],
                               'rawFlag': integer(raw['rawFlag'], 'rawFlag') if raw else 0xffffffff,
                               'rawOffset': raw['offset'] if raw else [0, 0], 'rawShadow': raw['shadow'] if raw else [0, 0],
                               'events': events, 'sourceMetadata': canonical_text({k: frame.get(k) for k in ('rawWan', 'sourceRGBAHash', 'sourceShadowHash')})})
            rows.append(frames)
        # Aliases retain source alias references AND their own WAN duration/flags/markers.
        packed_clips.append({'id': ids[clip['name']], 'sourceWanId': clip['wanId'] if asset['source']['sourceKind'] == 'rom' and clip['wanId'] is not None else -1,
                             'aliasOf': ids[clip['aliasOf']] if clip['aliasOf'] else -1,
                             'name': clip['name'], 'rows': rows,
                             'sourceMetadata': canonical_text({k: v for k, v in clip.items() if k not in ('directions', 'rawDirections')})})
    if len(provisional) != 1:
        raise ValueError('per-clip tick rates differ; unsupported package timing')
    return {'assetVersion': 1, 'tickHz': provisional.pop(), 'width': atlas['width'], 'height': atlas['height'],
            'pngBytes': atlas['byteSize'], 'sourceKind': {'test': 0, 'rom': 1, 'public': 2}[asset['source']['sourceKind']],
            'shadowSize': asset['shadowSize'], 'atlasName': 'atlas.png', 'pngSha256': atlas['sha256'], 'assetKey': asset['assetKey'],
            'provenance': canonical_text({'source': asset['source'], 'sourceManifestHash': manifest_hash,
                                         'shadowEncoding': asset['shadowEncoding'], 'compiler': 'tools/compile_runtime_package.py',
                                         'compilerHash': file_hash(Path(__file__)), 'timing': 'sourceTiming retained; provisional rate is not original wall-time verification'}),
            'clips': packed_clips}


def compile_package(source, output):
    validation = validate_package(source)
    manifest = read_json(source / 'manifest.json')
    results = []
    with staging(output) as temporary:
        for index, asset in enumerate(manifest['assets']):
            package = convert_asset(asset, validation['manifestHash'])
            encoded = encode_package(package)
            if len(encoded) > 2 * 1024 * 1024:
                raise ValueError('runtime manifest 2MiB budget exceeded')
            destination = temporary / f'asset-{index}'
            destination.mkdir()
            shutil.copyfile(safe_file(source, asset['atlas']['file']), destination / 'atlas.png')
            (destination / 'asset.mdpa').write_bytes(encoded)
            (destination / 'CREDITS.txt').write_text(canonical_text(asset['source']) + '\n', encoding='utf-8')
            results.append({'assetKey': asset['assetKey'], 'manifest': f'asset-{index}/asset.mdpa',
                            'sha256': hashlib.sha256(encoded).hexdigest(), 'atlasSha256': package['pngSha256']})
        report = {'status': 'compiled_requires_consumer_validation', 'sourceManifestHash': validation['manifestHash'],
                  'assets': results, 'originalWallTime': 'pending', 'WindowsExecution': 'pending'}
        write_json(temporary / 'runtime-index.json', report)
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(compile_package(args.package, args.out), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
