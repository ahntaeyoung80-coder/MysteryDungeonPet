"""Build isolated negative packages for native WIC loader checks."""
import copy
import hashlib
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from make_fixture import make_fixture
from package_format import encode_package

# Read the producer values by exposing a package-building helper rather than interpreting the runtime binary.
from make_fixture import fixture_manifest


def main():
    root = Path(sys.argv[1])
    png, valid = make_fixture()
    base = fixture_manifest(png)
    cases = {}
    cases['truncated'] = (valid[:-1], png)
    version = bytearray(valid)
    version[4:8] = (2).to_bytes(4, 'little')
    cases['schema'] = (bytes(version), png)
    manifest = copy.deepcopy(base)
    manifest['clips'][0]['aliasOf'] = 107
    manifest['clips'][0]['rows'] = []
    manifest['clips'][1]['aliasOf'] = 0
    manifest['clips'][1]['rows'] = []
    cases['alias-cycle'] = (encode_package(manifest), png)
    cases['missing-atlas'] = (valid, None)
    cases['hash'] = (valid, png[:-1] + b'x')
    manifest = copy.deepcopy(base)
    manifest['width'] = 49
    cases['dimensions'] = (encode_package(manifest), png)
    broken = png[:32]
    manifest = copy.deepcopy(base)
    manifest['pngBytes'] = len(broken)
    manifest['pngSha256'] = hashlib.sha256(broken).hexdigest()
    cases['corrupt-png-valid-hash'] = (encode_package(manifest), broken)
    manifest = copy.deepcopy(base)
    manifest['atlasName'] = '../atlas.png'
    cases['traversal'] = (encode_package(manifest), png)
    for name, (packed, image) in cases.items():
        directory = root / name
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'fixture.mdpa').write_bytes(packed)
        if image is not None:
            (directory / 'atlas.png').write_bytes(image)
    print(f'{len(cases)} negative packages generated')


if __name__ == '__main__':
    main()
