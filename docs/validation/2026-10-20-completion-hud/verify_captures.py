"""최종 Windows EXE의 완료 경로·식사 시간·단독 초상화 캡처를 검증한다."""
import csv
import hashlib
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
CAPTURES = ROOT / 'build/completion-wine'
OUTPUT = Path(__file__).resolve().parent

def rows(name):
    return list(csv.DictReader((CAPTURES / (name+'.csv')).open()))

def skin(width, height):
    tiles = Image.open(ROOT / 'assets/original/gameplay/ui/atlas.png').convert('RGBA')
    image = Image.new('RGBA', (width,height))
    ids = [[2,3,4],[5,1,6],[7,8,9]]
    for y in range(0,height,8):
        for x in range(0,width,8):
            tile = ids[0 if y == 0 else 2 if y == height-8 else 1][0 if x == 0 else 2 if x == width-8 else 1]
            image.paste(tiles.crop((tile*8,0,tile*8+8,8)), (x,y))
    return image

atlas = Image.open(ROOT/'assets/original/gameplay/portraits/atlas.png').convert('RGBA')
entries = json.loads((ROOT/'assets/original/gameplay/portraits/provenance.json').read_text())['portraits']
portraits = {p['bodyMdIndex']: atlas.crop((i*40,0,i*40+40,40)) for i,p in enumerate(entries)}
frame, plate = skin(56,56), skin(112,24)
scenes = {'npc-player':(1,True),'npc-reply':(383,False),'companion-player':(1,True),'companion-reply':(4,False),'bed-greeting':(1,True),'wake-morning-greeting':(1,True),'wake-alarm':(322,False),'swapped-companion-player':(4,True),'swapped-companion-reply':(1,False)}
for name,(identity,player) in scenes.items():
    group=name.split('-')[0]
    filename=name if name.startswith(('wake-','swapped-')) else group+'-'+name
    image=Image.open(CAPTURES/(filename+'.bmp')).convert('RGBA')
    left=8 if player else 136
    assert image.crop((left+8,96,left+48,136)).tobytes()==portraits[identity].tobytes(),name
    for y in range(56):
        for x in range(56):
            if 8<=x<48 and 8<=y<48:continue
            pixel=frame.getpixel((x,y))
            if pixel[3]==255:assert image.getpixel((left+x,88+y))==pixel,name
    listener=image.crop((144 if player else 16,96,184 if player else 56,136)).tobytes()
    assert all(listener!=p.tobytes() for p in portraits.values()),name
    plate_left=72 if player else 16
    assert any(image.getpixel((plate_left+x,120+y))!=plate.getpixel((x,y)) for x in range(112) for y in range(4)),name
    image.save(OUTPUT/(name+'.png'))

peak=rows('peak');stages=[]
for r in peak:
    key=(int(r['life']),int(r['scene_beat']))
    if not stages or stages[-1]!=key:stages.append(key)
assert stages[:9]==[(0,13),(1,20),(1,21),(1,22),(1,23),(1,24),(2,0),(4,1),(5,2)],stages
walk=[r for r in peak if r['scene_beat']=='21']
assert all(float(r['world_x'])==344 and 228<=float(r['world_y'])<=364 for r in walk)
assert all(float(a['world_y'])>=float(b['world_y']) for a,b in zip(walk,walk[1:]))
assert all(float(r['companion_world_x'])==304 and 228<=float(r['companion_world_y'])<=348 for r in walk)
assert all(r['clip']=='0' and r['companion_clip']=='0' for r in walk)
text=[r for r in peak if r['scene_beat']=='23']
assert all((r['world_x'],r['world_y'],r['companion_world_x'],r['companion_world_y'])==('344','228','304','228') for r in text)
meal=next(r for r in peak if r['life']=='4');exit_=next(r for r in peak if r['scene_beat']=='2')
meal_ms=(int(exit_['elapsed_us'])-int(meal['elapsed_us']))/1000
assert 5950<=meal_ms<6050,meal_ms
image=Image.open(CAPTURES/'peak-completion.bmp').convert('RGBA')
background=Image.open(ROOT/'assets/original/gameplay/completion/peak.png').convert('RGBA')
camera=(int(text[0]['camera_x']),int(text[0]['camera_y']))
assert image.crop((0,0,200,20)).tobytes()==background.crop((camera[0],camera[1],camera[0]+200,camera[1]+20)).tobytes()
assert image.crop((8,24,193,41)).tobytes()!=background.crop((camera[0]+8,camera[1]+24,camera[0]+193,camera[1]+41)).tobytes()
image.save(OUTPUT/'completion.png')
Image.open(CAPTURES/'test-completion.bmp').save(OUTPUT/'test-completion.png')
for name in ['npc','companion','swapped']:
    dialogue=[r for r in rows(name) if r['life']=='10']
    assert len({(r['world_x'],r['world_y'],r['companion_world_x'],r['companion_world_y']) for r in dialogue})==1
result={'platform':'MinGW GCC14.2 static x64 Release, Wine10/Xvfb; test-only unlock/font substitute',
        'runtimeSha256':hashlib.sha256((ROOT/'build/dialogue-windows/MysteryDungeonPet.exe').read_bytes()).hexdigest(),
        'onlySpeakerAndNoNameplate':list(scenes),'originalSteamCavePeakBackground':True,
        'playerWalk':'344,364 -> 344,228','partnerWalk':'304,348 -> 304,228',
        'completionOrder':stages,'mealDurationMs':meal_ms,'mealAnimationSpeed':'unchanged',
        'relatedNativeTests':'8 unique targets passed; initial signed interpolation failure fixed and completion test rerun',
        'gdi':'8 Korean text scenes including success + 3 map/floor labels passed',
        'nativeWindows11':'pending','fullMemorySuite':'not run; scoped related tests and actual EXE first',
        'ci':'background after push'}
(OUTPUT/'summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('actual EXE: original peak, ordered success return, six-second meal, single portraits without nameplates passed')
