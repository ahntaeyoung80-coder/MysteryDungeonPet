"""기존 A-1 패키지→MDPA→native 메타데이터·재생 계약 검증. ROM 재추출은 하지 않는다."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.compile_runtime_package import POINTS, EVENTS
from tools.mdpet.common import file_hash, read_json, safe_file, write_json
from tools.mdpet.packer import validate_package


def require(condition, detail):
    if not condition:
        raise ValueError(detail)


def native_json(executable, path):
    return json.loads(subprocess.check_output([str(executable.resolve()), str(path.resolve())], text=True, timeout=60))


def verify(source, runtime, inspector, playback):
    validation = validate_package(source)
    manifest = read_json(source / 'manifest.json')
    runtime_index = read_json(runtime / 'runtime-index.json')
    require(runtime_index['sourceManifestHash'] == validation['manifestHash'], 'runtime source manifest differs')
    require(len(runtime_index['assets']) == len(manifest['assets']), 'runtime asset count differs')
    results = []
    for asset, record in zip(manifest['assets'], runtime_index['assets']):
        path = safe_file(runtime, record['manifest'])
        require(file_hash(path) == record['sha256'], 'runtime MDPA hash differs')
        require(file_hash(path.parent / 'atlas.png') == asset['atlas']['sha256'] == record['atlasSha256'], 'atlas hash differs')
        native = native_json(inspector, path)
        require(bytes.fromhex(native['assetKeyHex']).decode() == asset['assetKey'] == record['assetKey'], 'asset key differs')
        require(native['sourceKind'] == {'test': 0, 'rom': 1, 'public': 2}[asset['source']['sourceKind']], 'sourceKind differs')
        require(native['shadowSize'] == asset['shadowSize'], 'shadowSize differs')
        require(native['assetVersion'] == manifest['assetVersion'], 'assetVersion differs')
        for field in ('width', 'height'):
            require(native[field] == asset['atlas'][field], f'atlas {field} differs')
        require(native['pngBytes'] == asset['atlas']['byteSize'] and native['pngSha256'] == asset['atlas']['sha256'], 'native atlas binding differs')
        require(bytes.fromhex(native['atlasNameHex']).decode() == 'atlas.png', 'native atlas filename differs')
        provenance = json.loads(bytes.fromhex(native['provenanceHex']))
        require(provenance['sourceManifestHash'] == validation['manifestHash'] and provenance['source'] == asset['source'], 'native provenance differs')
        require(len(native['clips']) == len(asset['animations']), 'native clip count differs')
        by_id = {clip['id']: clip for clip in native['clips']}
        by_name = {clip['name']: clip for clip in asset['animations']}
        frame_count = 0
        for clip in asset['animations']:
            # 대표 원작 package의 WAN 선택ID를 사용한다. 알 수 없는 ID는 추측하지 않는다.
            require(clip['wanId'] in by_id, 'source WAN selector absent in native package')
            actual = by_id[clip['wanId']]
            require(actual['sourceWanId'] == (clip['wanId'] if asset['source']['sourceKind'] == 'rom' else -1), 'WAN provenance differs')
            require(actual['aliasOf'] == (by_name[clip['aliasOf']]['wanId'] if clip['aliasOf'] else -1), 'alias differs')
            metadata = json.loads(bytes.fromhex(actual['sourceMetadataHex']))
            require(metadata['sourceTiming'] == clip['sourceTiming'], 'raw timing metadata differs')
            timing = clip['sourceTiming']
            require(native['tickHz'] == (timing.get('tickHz') or timing.get('provisionalTickHz')), 'native tickHz differs')
            require(actual['speedNumerator'] == actual['speedDenominator'] == 1 and actual['rawFlags'] == 0, 'unsupported native speed/clip flags')
            require(bytes.fromhex(actual['nameHex']).decode() == clip['name'], 'native clip name differs')
            target = clip
            while target['aliasOf']:
                target = by_name[target['aliasOf']]
            require(len(actual['rows']) == len(target['directions']), 'direction rows differ')
            raw_alias = clip.get('rawDirections')
            for direction, frames in enumerate(actual['rows']):
                raw_row = raw_alias[direction] if raw_alias else None
                require(len(frames) == (len(raw_row) if raw_row else len(target['directions'][direction])), 'direction frame count differs')
                for index, frame in enumerate(frames):
                    original = target['directions'][direction][index]
                    raw = raw_row[index] if raw_row else original.get('rawWan')
                    for field in ('rect', 'shadowRect', 'anchor', 'cropOrigin'):
                        require(frame[field] == original[field], f'{field} differs')
                    require(frame['shadow'] == (original['points']['shadow'] or [0,0]), 'shadow point differs')
                    require(frame['ticks'] == (raw['durationTicks'] if raw else original['durationTicks']), 'ticks differ')
                    require(frame['rawFlag'] == (raw['rawFlag'] if raw else 0xffffffff), 'raw flag differs')
                    require(frame['rawOffset'] == (raw['offset'] if raw else [0,0]), 'raw offset differs')
                    require(frame['rawShadow'] == (raw['shadow'] if raw else [0,0]), 'raw shadow differs')
                    events = raw['markers'] if raw else original['markers']
                    require(frame['events'] == sum(1 << EVENTS.index(event) for event in events), 'event flags differ')
                    expected_points = [{'kind': POINTS.index(name), 'point': point} for name, point in original['points'].items() if point is not None]
                    require(sorted(frame['markers'], key=lambda item: item['kind']) == sorted(expected_points, key=lambda item: item['kind']), 'marker points differ')
                    meta = json.loads(bytes.fromhex(frame['sourceMetadataHex']))
                    require(meta['rawWan'] == raw and meta['sourceRGBAHash'] == original['sourceRGBAHash'] and meta['sourceShadowHash'] == original['sourceShadowHash'], 'frame source hashes/metadata differ')
                    frame_count += 1
        replay = native_json(playback, path)
        require(replay['status'] == 'pass', 'native playback failed')
        results.append({'assetKey': asset['assetKey'], 'sourceKind': asset['source']['sourceKind'],
                        'framesCompared': frame_count, 'MDPASHA256': file_hash(path),
                        'atlasSHA256': asset['atlas']['sha256'], 'nativePlayback': replay})
    return {'schemaVersion': 1, 'status': 'pass', 'sourceManifestSHA256': validation['manifestHash'],
            'assets': results, 'framesCompared': sum(item['framesCompared'] for item in results),
            'scope': 'existing A-1 RGBA reassembly, A-1/MDPA/native metadata and native tick playback',
            'ROMReextraction': 'not_performed', 'originalWallTime': 'pending', 'WindowsOriginalIntegration': 'pending', 'aComplete': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--inspector', type=Path, required=True)
    parser.add_argument('--playback', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = args.report.resolve()
    require(not any(report.is_relative_to(path.resolve()) for path in (args.package, args.runtime)), 'report must be outside input packages')
    result = verify(args.package, args.runtime, args.inspector, args.playback)
    write_json(args.report, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
