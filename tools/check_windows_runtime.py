"""Keep the directly runnable Windows binary tied to its tracked source and assets."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'MysteryDungeonPet.exe'
MANIFEST = ROOT / 'windows-runtime.json'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inputs():
    sources = [ROOT / 'CMakeLists.txt', ROOT / 'cmake/mingw.cmake']
    sources += sorted(p for p in (ROOT / 'src').rglob('*') if p.is_file())
    assets = []
    for folder in ('runtime', 'exploration', 'gameplay'):
        assets += sorted(p for p in (ROOT / 'assets/original' / folder).rglob('*') if p.is_file())
    result = {}
    for path in sources + assets:
        data = path.read_bytes()
        if path.suffix in {'.cpp', '.hpp', '.inc', '.txt', '.cmake', '.json', '.manifest', '.rc', '.md'}:
            data = data.replace(b'\r\n', b'\n')  # Git for Windows may check out text with CRLF.
        result[path.relative_to(ROOT).as_posix()] = hashlib.sha256(data).hexdigest()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--refresh-from', type=Path, help='Copy an already built Windows x64 EXE; no ZIP step')
    parser.add_argument('--compiler', help='Compiler/version used for the refreshed EXE')
    args = parser.parse_args()
    if args.refresh_from:
        if not args.compiler:
            parser.error('--compiler is required when refreshing')
        data = args.refresh_from.read_bytes()
    else:
        data = EXE.read_bytes()
    if data[:2] != b'MZ':
        raise SystemExit('Windows runtime is not a PE executable')
    pe_offset = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe_offset:pe_offset+4] != b'PE\0\0' or struct.unpack_from('<H', data, pe_offset+4)[0] != 0x8664:
        raise SystemExit('Windows runtime must target x64')
    if args.refresh_from:
        shutil.copyfile(args.refresh_from, EXE)
        EXE.chmod(0o644)
        record = {'compiler': args.compiler, 'exe_sha256': digest(EXE), 'inputs': inputs(),
                  'entrypoint': 'MysteryDungeonPet.exe', 'native_windows_execution': 'see validation records'}
        MANIFEST.write_text(json.dumps(record, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    else:
        record = json.loads(MANIFEST.read_text(encoding='utf-8'))
        if record['exe_sha256'] != digest(EXE) or record['inputs'] != inputs():
            raise SystemExit('Windows runtime is stale: rebuild and refresh EXE + windows-runtime.json before committing')
    print('Windows x64 runtime/source/assets hashes match; no ZIP required')


if __name__ == '__main__':
    main()
