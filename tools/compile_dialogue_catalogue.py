"""작성한 상황 문답을 검증하고 두 줄 페이지의 MDLC1로 변환한다."""
import argparse
import json
from pathlib import Path
import re
import struct

ROLES = {'companion.checkin': [(1, 2, 11), (2, 1, 12)], 'npc.greet': [(1, 3, 21), (3, 1, 22)],
         'sleep.greet': [(1, 2, 31)], 'wake.alarm': [(4, 1, 41)], 'morning.depart': [(1, 2, 51)],
         'defeat.player': [(2, 1, 101)], 'defeat.partner': [(2, 1, 102)]}


def pages(text):
    if not text or len(text) > 1024 or any(ord(c) < 32 or 0xD800 <= ord(c) <= 0xDFFF or ord(c) > 0xFFFF for c in text):
        raise ValueError('unsupported/empty dialogue text')
    lines, line, units = [], '', 0
    for char in text:
        width = 1 if ord(char) < 128 else 2
        if units + width > 26:
            lines.append(line.rstrip())
            line, units = '', 0
        if not line and char == ' ':
            continue
        line += char
        units += width
    if line:
        lines.append(line.rstrip())
    return ['\n'.join(lines[i:i + 2]) for i in range(0, len(lines), 2)]


def compile_catalogue(source):
    if source['schemaVersion'] != 1 or not 0 < source['catalogueVersion'] < 2**32:
        raise ValueError('unsupported catalogue version')
    bundles = source['bundles']
    if not 0 < len(bundles) <= 128:
        raise ValueError('bundle limit')
    data = bytearray(struct.pack('<4I', 0x434C444D, 1, source['catalogueVersion'], len(bundles)))
    def u32(n):
        data.extend(struct.pack('<I', n))
    def string(s):
        b = s.encode('utf-8')
        if not 0 < len(b) <= 256 or '\0' in s:
            raise ValueError('dialogue string limit')
        u32(len(b)); data.extend(b)
    def identity(s):
        if not re.fullmatch(r'[A-Za-z0-9_.-]{1,128}', s):
            raise ValueError('invalid dialogue ID')
        string(s)
    seen, ids = set(), set()
    for bundle in bundles:
        context = bundle['context']
        if context not in ROLES or bundle['condition'] != context or bundle['style'] != 'neutral':
            raise ValueError('unverified context/condition/style')
        if bundle['id'] in seen or not 0 < bundle['weight'] <= 100:
            raise ValueError('duplicate bundle/invalid weight')
        seen.add(bundle['id'])
        lines = bundle['lines']
        if [(l['speaker'], l['listener'], l['legacyId']) for l in lines] != ROLES[context]:
            raise ValueError('context speaker/line sequence mismatch')
        expanded = []
        for line in lines:
            if not 0 <= line['emotion'] < 14 or not 700 <= line['holdMs'] <= 5000:
                raise ValueError('invalid emotion/reading time')
            for i, text in enumerate(pages(line['text']), 1):
                expanded.append(dict(line, id=f'{line["id"]}.p{i}', text=text))
        if len(expanded) > 32:
            raise ValueError('page limit')
        identity(bundle['id']); identity(context); u32(bundle['weight']); u32(len(expanded))
        for line in expanded:
            if line['id'] in ids:
                raise ValueError('duplicate line ID')
            ids.add(line['id']); identity(line['id'])
            for key in ('legacyId', 'speaker', 'listener', 'emotion', 'holdMs'):
                u32(line[key])
            string(line['text'])
    return bytes(data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('assets/original/gameplay/dialogue/catalogue.json'))
    parser.add_argument('--out', type=Path, default=Path('assets/original/gameplay/dialogue/catalogue.mdlc'))
    args = parser.parse_args()
    data = compile_catalogue(json.loads(args.source.read_text(encoding='utf-8')))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f'MDLC1: {len(data)} bytes')
