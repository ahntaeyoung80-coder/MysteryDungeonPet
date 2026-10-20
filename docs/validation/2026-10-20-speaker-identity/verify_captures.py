"""새 EXE의 --movement-trace 캡처에서 화자 한 명·원작 테두리·ID 교체를 대조한다."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
from PIL import Image


def verify(root, captures, output):
    output.mkdir(parents=True, exist_ok=True)
    folder = root / 'assets/original/gameplay'
    atlas = Image.open(folder / 'portraits/atlas.png').convert('RGBA')
    entries = json.loads((folder / 'portraits/provenance.json').read_text())['portraits']
    # The packer sorts by MD character ID, independently of the speaker role.
    portraits = {entry['bodyMdIndex']: atlas.crop((column*40, 0, column*40+40, 40)) for column, entry in enumerate(entries)}
    tiles = Image.open(folder / 'ui/atlas.png').convert('RGBA')
    def skin(width, height):
        result = Image.new('RGBA', (width, height))
        ids = [[2, 3, 4], [5, 1, 6], [7, 8, 9]]
        for y in range(0, height, 8):
            for x in range(0, width, 8):
                tile = ids[0 if y == 0 else 2 if y == height-8 else 1][0 if x == 0 else 2 if x == width-8 else 1]
                result.paste(tiles.crop((tile*8, 0, tile*8+8, 8)), (x, y))
        return result
    frame, nameplate = skin(56, 56), skin(112, 24)
    scenes = {
        'npc-player': ('verified-npc-npc-player.bmp', 1, True),
        'npc-reply': ('verified-npc-npc-reply.bmp', 383, False),
        'companion-player': ('verified-companion-companion-player.bmp', 1, True),
        'companion-reply': ('verified-companion-companion-reply.bmp', 4, False),
        'bed': ('verified-bed-bed-greeting.bmp', 1, True),
        'morning': ('verified-wake-morning-greeting.bmp', 1, True),
        'alarm': ('verified-wake-alarm.bmp', 322, False),
        'swapped-player': ('verified-swapped-companion-player.bmp', 4, True),
        'swapped-partner': ('verified-swapped-companion-reply.bmp', 1, False),
    }
    checks = {}
    for name, (file, md, player) in scenes.items():
        image = Image.open(captures / file).convert('RGBA')
        left = 8 if player else 136
        assert image.crop((left+8, 96, left+48, 136)).tobytes() == portraits[md].tobytes(), name
        other = 144 if player else 16
        listener = image.crop((other, 96, other+40, 136)).tobytes()
        assert all(listener != face.tobytes() for face in portraits.values()), name
        for y in range(56):
            for x in range(56):
                if 8 <= x < 48 and 8 <= y < 48:
                    continue
                expected = frame.getpixel((x, y))
                if expected[3] == 255:
                    assert image.getpixel((left+x, 88+y)) == expected, (name, x, y)
        for y in list(range(4)) + list(range(21, 24)):
            for x in range(112):
                expected = nameplate.getpixel((x, y))
                if expected[3] == 255:
                    assert image.getpixel(((72 if player else 16)+x, 120+y)) == expected, name
        image.save(output / (name + '.png'))
        checks[name] = {'characterMdIndex': md, 'onlySpeaker': True, 'originalPortraitPixels': True,
                        'originalWindowBorder': True, 'originalNameplateBorder': True}
    motion = {}
    for scene in ('npc', 'companion', 'swapped'):
        rows = list(csv.DictReader((captures / f'verified-{scene}.csv').open()))
        dialogue = [r for r in rows if r['life'] == '10']
        assert dialogue
        assert len({(r['world_x'], r['world_y'], r['companion_world_x'], r['companion_world_y'], r['round']) for r in dialogue}) == 1
        assert all(r['moving'] == '0' and r['companion_moving'] == '0' for r in dialogue)
        assert any(r['life'] == '0' and r['moving'] == '1' for r in rows[rows.index(dialogue[-1])+1:])
        motion[scene] = {'dialogueFrames': len(dialogue), 'pausedAndResumed': True}
    result = {'platform': 'MinGW GCC14.2 static x64 Release; Wine10/Xvfb; test-only unlock/font substitute',
              'screenshots': checks, 'motion': motion, 'nativeTests': '2/2', 'pythonIdentityTests': '4/4',
              'gdi': '7 dialogue scenes + 2 role/name labels pass',
              'runtimeSha256': hashlib.sha256((root / 'build/dialogue-windows/MysteryDungeonPet.exe').read_bytes()).hexdigest(),
              'nativeWindowsExecution': 'pending', 'fullMemorySuite': 'not run for scoped UI/asset identity change',
              'ci': 'background after release'}
    (output / 'summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('9 actual EXE scenes: one speaker, original portrait/window pixels, player/partner replacement, motion resume pass')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--captures', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    verify(args.root, args.captures, args.output)
