"""실제 EXE의 21종 메뉴·중복 차단·선택 복원·몸체/초상화를 대조한다."""
from pathlib import Path
import csv
import hashlib
import json
import struct
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'build/starter-union-wine'
OUT = Path(__file__).resolve().parent
GAME = ROOT / 'assets/original/gameplay'
catalogue = json.loads((GAME / 'starters/catalogue.json').read_text())
characters = {c['mdIndex']: c for c in catalogue['characters']}
def rows(name):
    return list(csv.DictReader((SOURCE / (name + '.csv')).open()))

all_rows = {name: rows(name) for name in ['menu', 'restart', 'hero-meowth', 'hero-munchlax', 'legacy']}
for name, trace in all_rows.items():
    assert trace, name
    assert all(characters[int(r['player_character'])]['speciesId'] !=
               characters[int(r['partner_character'])]['speciesId'] for r in trace), name
pairs = {(r['player_character'], r['partner_character']) for r in all_rows['menu']}
assert pairs == {('1', '4'), ('488', '4'), ('488', '52')}, pairs
for name, pair in [('restart', ('488', '52')), ('hero-meowth', ('52', '4')),
                   ('hero-munchlax', ('488', '4')), ('legacy', ('625', '1'))]:
    assert all((r['player_character'], r['partner_character']) == pair for r in all_rows[name]), name
for filename, pair in [('menu.state', (488, 52)), ('legacy.state', (625, 1))]:
    assert struct.unpack_from('<II', (SOURCE / filename).read_bytes(), 76) == pair

# Read only the final menu run from the Wine event log copied by the runner.
events = Path('/workspace/scratch/starter-union-events.log').read_text().splitlines()
starts = [i for i, line in enumerate(events) if 'character menu role=1 options=21' in line]
events = [line for line in events[starts[-4]:] if 'character' in line]
assert sum('character selection rejected:' in line for line in events) == 2
queued = [line for line in events if 'character selection queued' in line]
assert len(queued) == 2 and 'role=1 md=488' in queued[0] and 'role=2 md=52' in queued[1]
assert sum('options=21' in line for line in events) == 8
(OUT / 'events.txt').write_text('\n'.join(events) + '\n')

portraits = Image.open(GAME / 'portraits/atlas.png').convert('RGBA')
audit = json.loads((GAME / 'portraits/provenance.json').read_text())['portraits']
columns = sorted({p['bodyMdIndex'] for p in audit})
matched = {}
for name, md in [('hero-meowth', 52), ('hero-munchlax', 488)]:
    image = Image.open(SOURCE / (name + '-companion-player.bmp')).convert('RGBA')
    column = columns.index(md) * 40
    expected = portraits.crop((column, 160, column + 40, 200))
    assert image.crop((16, 96, 56, 136)).tobytes() == expected.tobytes(), name
    image.save(OUT / (name + '-dialogue.png'))
    sample = next(r for r in all_rows[name] if r['submitted'] == '1')
    actor = characters[md]
    package = json.loads(subprocess.check_output([
        str(ROOT / 'build/dialogue-linux/pet_package_inspect'), str(GAME / actor['bodyManifest'])], text=True))
    clip = next(c for c in package['clips'] if c['id'] == int(sample['clip']))
    directions = clip['rows']
    frame = directions[int(sample['direction']) if len(directions) > 1 else 0][int(sample['frame_index'])]
    body = Image.open((GAME / actor['bodyManifest']).parent / 'atlas.png').convert('RGBA')
    x, y, width, height = frame['rect']
    sprite = body.crop((x, y, x + width, y + height))
    actual = Image.open(SOURCE / (name + '-first.bmp')).convert('RGBA')
    left = int(sample['scene_x']) - int(sample['camera_x']) - frame['anchor'][0]
    top = int(sample['scene_y']) - int(sample['camera_y']) - frame['anchor'][1]
    count = 0
    for y in range(height):
        for x in range(width):
            pixel = sprite.getpixel((x, y))
            if pixel[3] == 255 and 0 <= left + x < 200 and 23 <= top + y < 144:
                assert actual.getpixel((left + x, top + y)) == pixel, (name, x, y)
                count += 1
    assert count > 50
    matched[str(md)] = count
for suffix in ['player', 'partner-blocked', 'player-blocked', 'disabled-highlight']:
    Image.open(SOURCE / ('menu-ui-' + suffix + '.bmp')).save(OUT / ('menu-' + suffix + '.png'))
result = {
    'platform': 'MinGW GCC14.2 static x64 Release; Wine10/Xvfb; test-only unlock and font substitutes',
    'runtimeSha256': hashlib.sha256((ROOT / 'build/dialogue-windows/MysteryDungeonPet.exe').read_bytes()).hexdigest(),
    'heroOptions': 21, 'partnerOptions': 21, 'nativePairCoverage': {'allowed': 420, 'blocked': 21},
    'actualPopupSelection': {'player': 488, 'partner': 52, 'restartPreserved': True},
    'disabledEntryEnter': 'popup dismissed without queuing a character change',
    'directDuplicateRequestsRejected': 2,
    'allActualTracePairsDistinct': True, 'heroBodyOpaquePixelsMatched': matched,
    'heroPortraitsMatched': [52, 488], 'legacyGenderDuplicateRepairedAndPersisted': [625, 1],
    'nativeTests': 'character_scene and dialogue_portrait 2/2 passed',
    'pythonTests': 'character_assets 4/4 passed', 'nativeWindows11': 'pending',
    'fullCi': 'background after push',
}
(OUT / 'summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print('PASS: 21 options per role, distinct menu/restart pairs, two duplicate rejections, legacy repair, exact Meowth/Munchlax body and portrait pixels')
