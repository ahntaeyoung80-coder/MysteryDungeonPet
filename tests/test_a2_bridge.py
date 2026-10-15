import copy
import json
import os
from pathlib import Path
import subprocess
import hashlib
import struct
import tempfile
import unittest
from tools.compile_runtime_package import compile_package, convert_asset, POINTS, EVENTS
from tools.mdpet.common import read_json, write_json
from tools.mdpet.fixture import create_sheets, provenance
from tools.mdpet.packer import pack_asset, write_package
from tools.verify_a3_package import verify


class A2Bridge(unittest.TestCase):
    def test_a1_to_native_preserves_every_frame_and_alias_timing(self):
        inspector = os.environ.get('PET_PACKAGE_INSPECT')
        if not inspector:
            self.skipTest('set PET_PACKAGE_INSPECT to the built native inspector for A-1/A-2 integration')
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            original = root / 'a1'
            original.mkdir()
            create_sheets(original / 'sheets')
            raw = read_json(original / 'sheets/raw-wan.json')
            # The alias owns different timing/flags/markers; sharing the target must not erase these.
            raw['7'][0][0]['durationTicks'] = 41
            raw['7'][0][0]['rawFlag'] = 7
            raw['7'][0][0]['markers'] = ['rush', 'return']
            asset, support = pack_asset(original / 'sheets', original, 'test:fixture:shapes', provenance(), raw)
            write_package(original, [asset], [support])
            result = compile_package(original, root / 'a2')
            native = json.loads(subprocess.check_output([inspector, str(root / 'a2/asset-0/asset.mdpa')], text=True))
            self.assertEqual(bytes.fromhex(native['assetKeyHex']).decode(), asset['assetKey'])
            self.assertEqual(native['sourceKind'], 0)
            self.assertEqual(native['shadowSize'], asset['shadowSize'])
            provenance_data = json.loads(bytes.fromhex(native['provenanceHex']))
            self.assertEqual(provenance_data['sourceManifestHash'], result['sourceManifestHash'])
            self.assertEqual(provenance_data['source'], asset['source'])
            by_id = {clip['id']: clip for clip in native['clips']}
            by_name = {clip['name']: clip for clip in asset['animations']}
            compared = 0
            for clip in asset['animations']:
                c = by_id[clip['wanId']]
                target = clip
                while target['aliasOf']:
                    target = by_name[target['aliasOf']]
                self.assertEqual(c['aliasOf'], by_name[clip['aliasOf']]['wanId'] if clip['aliasOf'] else -1)
                self.assertEqual(c['sourceWanId'], -1)
                clip_meta = json.loads(bytes.fromhex(c['sourceMetadataHex']))
                self.assertEqual(clip_meta['sourceTiming'], clip['sourceTiming'])
                raw_alias = clip.get('rawDirections')
                self.assertEqual(len(c['rows']), len(target['directions']))
                for direction, frames in enumerate(c['rows']):
                    raws = raw_alias[direction] if raw_alias else None
                    expected_count = len(raws) if raws else len(target['directions'][direction])
                    self.assertEqual(len(frames), expected_count)
                    for index, frame in enumerate(frames):
                        source = target['directions'][direction][index]
                        raw_frame = raws[index] if raws else source['rawWan']
                        for field in ('rect', 'shadowRect', 'anchor', 'cropOrigin'):
                            self.assertEqual(frame[field], source[field])
                        self.assertEqual(frame['shadow'], source['points']['shadow'])
                        self.assertEqual(frame['ticks'], raw_frame['durationTicks'])
                        self.assertEqual(frame['rawFlag'], raw_frame['rawFlag'])
                        self.assertEqual(frame['rawOffset'], raw_frame['offset'])
                        self.assertEqual(frame['rawShadow'], raw_frame['shadow'])
                        self.assertEqual(frame['events'], sum(1 << EVENTS.index(event) for event in raw_frame['markers']))
                        self.assertEqual(sorted(frame['markers'], key=lambda marker: marker['kind']), sorted([{'kind': POINTS.index(key), 'point': value} for key, value in source['points'].items() if value is not None], key=lambda marker: marker['kind']))
                        meta = json.loads(bytes.fromhex(frame['sourceMetadataHex']))
                        self.assertEqual(meta['rawWan'], raw_frame)
                        self.assertEqual(meta['sourceRGBAHash'], source['sourceRGBAHash'])
                        self.assertEqual(meta['sourceShadowHash'], source['sourceShadowHash'])
                        compared += 1
            self.assertEqual(by_id[7]['rows'][0][0]['ticks'], 41)
            self.assertEqual(by_id[7]['rows'][0][0]['events'], 5)
            self.assertGreater(compared, 100)
            native_path = Path(inspector)
            replay_path = native_path.with_name('pet_package_playback' + native_path.suffix)
            verified = verify(original, root / 'a2', native_path, replay_path)
            self.assertEqual(verified['framesCompared'], compared)
            self.assertFalse(verified['aComplete'])
            self.assertEqual(verified['assets'][0]['nativePlayback']['status'], 'pass')
            runtime_file = root / 'a2/asset-0/asset.mdpa'
            original_bytes = runtime_file.read_bytes()
            changed = bytearray(original_bytes)
            struct.pack_into('<I', changed, 12, 61)
            runtime_file.write_bytes(changed)
            changed_index = copy.deepcopy(result)
            changed_index['assets'][0]['sha256'] = hashlib.sha256(changed).hexdigest()
            write_json(root / 'a2/runtime-index.json', changed_index)
            with self.assertRaisesRegex(ValueError, 'tickHz'):
                verify(original, root / 'a2', native_path, replay_path)
            runtime_file.write_bytes(original_bytes)
            write_json(root / 'a2/runtime-index.json', result)
            second = compile_package(original, root / 'a2-again')
            self.assertEqual(result, second)
            self.assertEqual((root/'a2/asset-0/asset.mdpa').read_bytes(), (root/'a2-again/asset-0/asset.mdpa').read_bytes())
            with self.assertRaises(FileExistsError):
                compile_package(original, root / 'a2')
            print(f'A1->MDPA->native: {compared} frames (including aliases), points, flags, markers, ticks, hashes pass')

    def test_unsupported_budget_and_alias_geometry_are_explicit(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            create_sheets(root / 'sheets')
            asset, _ = pack_asset(root / 'sheets', root, 'test:fixture:shapes', provenance(), read_json(root/'sheets/raw-wan.json'))
            oversized = copy.deepcopy(asset)
            oversized['atlas']['decodedBytes'] = 8 * 1024 * 1024 + 4
            with self.assertRaisesRegex(ValueError, 'budget'):
                convert_asset(oversized, 'a' * 64)
            geometry = copy.deepcopy(asset)
            next(c for c in geometry['animations'] if c['name'] == 'Idle')['rawDirections'][0][0]['offset'][0] = 99
            with self.assertRaisesRegex(ValueError, 'geometry'):
                convert_asset(geometry, 'a' * 64)


if __name__ == '__main__':
    unittest.main()
