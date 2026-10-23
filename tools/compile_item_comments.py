"""검토용 아이템 반응 원문을 runtime C++ 표로 컴파일한다."""
import json
from pathlib import Path
from tools.compile_dialogue_catalogue import pages
ROOT = Path(__file__).resolve().parents[1]
def compile_comments(source):
    if source['schemaVersion'] != 1:
        raise ValueError('unsupported item comments')
    rows, seen = ['// Generated from assets/original/gameplay/dialogue/item_comments.json.'], set()
    for item in source['items']:
        key, lines = item['id'], item['lines']
        if key in seen or not 0 < key < 65536 or len(lines) != 3 or len(set(lines)) != 3:
            raise ValueError('item identity/variation mismatch')
        seen.add(key)
        for text in lines:
            if len(pages(text)) != 1:
                raise ValueError(f'item {key} exceeds two rows: {text}')
        quote = lambda text: 'L' + json.dumps(text, ensure_ascii=False)
        rows.append(f'case {key}:return variant==1?{quote(lines[1])}:variant==2?{quote(lines[2])}:{quote(lines[0])};')
    return '\n'.join(rows)+'\n'
if __name__ == '__main__':
    source=json.loads((ROOT/'assets/original/gameplay/dialogue/item_comments.json').read_text())
    (ROOT/'src/core/item_comments.inc').write_text(compile_comments(source))
    print(f'{len(source["items"])} items x 3 comments')
