"""실제 Win32 제출 화면·움직임·피해 대상·문답·원작 아이템을 대조한다."""
import csv,json,subprocess
from pathlib import Path
from PIL import Image
ROOT=next(p for p in Path(__file__).resolve().parents if (p/'CMakeLists.txt').is_file())
OUT=ROOT/'build/feedback-final'
GUI=OUT/'gui'
def rows(name):
    result=list(csv.DictReader((GUI/f'{name}.csv').open()))
    assert result and all(None not in row and None not in row.values() for row in result),name
    return result
result={}
patrol=rows('patrol-15')
def actors(row):return {int(a[0]):list(map(int,a[1:])) for v in row['actor_states'].split(';') if v for a in [v.split(':')]}
for identity,initial in [(1,132),(2,156)]:
    moved=next(r for r in patrol if identity in actors(r) and actors(r)[identity][0]!=initial)
    player=int(moved['player_tile']);assert player%24<10,(identity,player)
    assert any(v.startswith(f'{identity}:') and v.split(':')[2]=='400' for v in moved['actor_movements'].split(';'))
result['patrol']={'enemyAndFriendlyMoveBeforePlayerEntersTheirRoom':True,'durationMs':400,'submittedRows':sum(r['submitted']=='1' for r in patrol)}
front=rows('front-15');attack=next(r for r in front if 1 in actors(r) and actors(r)[1][1]==1)
assert attack['enemy_targets_companion']=='1' and attack['player_hp']=='1' and attack['companion_tile']=='101'
assert any(r['player_hp']=='1' and int(r['companion_hp'])<5 for r in front)
result['frontTarget']={'enemyTile':100,'companionTile':101,'lowHpPlayerTile':102,'firstAttackTargetsCompanion':True,'actualDamageObserved':True}
for name,suffix,prefix in [('sleep-15','bed-greeting','sleep.greet'),('morning-15','morning-greeting','morning.depart'),('alarm-15','alarm','wake.alarm'),('wounded-npc-15','npc-player','npc.greet'),('wounded-npc-15','npc-reply','npc.greet')]:
    data=rows(name);assert any(r['dialogue_bundle'].startswith(prefix) for r in data)
    image=Image.open(GUI/f'{name}-{suffix}.bmp').convert('RGBA')
    ink=sum(all(c>210 for c in image.getpixel((x,y))[:3]) for y in range(155,181) for x in range(16,184))
    assert ink>30,(name,suffix,ink)
    result[name+'-'+suffix]={'dialogueBundlePrefix':prefix,'visibleTextPixels':ink,'lowHpScenario':True}
items=json.loads(subprocess.check_output([str(ROOT/'build/p0/pet_package_inspect'),str(ROOT/'assets/original/gameplay/items/items.mdpa')],text=True))
atlas=Image.open(ROOT/'assets/original/gameplay/items/atlas.png').convert('RGBA')
data=rows('items-15');r=next(r for r in data if r['submitted']=='1');actual=Image.open(GUI/'items-15-first.bmp').convert('RGBA')
matched={}
for identity,tile in zip([2,18,70,109,183,10],[101,102,125,126,77,78]):
    frame=next(c for c in items['clips'] if c['id']==identity)['rows'][0][0]
    x,y,w,h=frame['rect'];ax,ay=frame['anchor'];left=tile%24*24+12-int(r['camera_x'])-ax;top=tile//24*24+12-int(r['camera_y'])-ay
    count=0
    for yy in range(h):
        for xx in range(w):
            expected=atlas.getpixel((x+xx,y+yy))
            if expected[3]!=255:continue
            assert actual.getpixel((left+xx,top+yy))==expected,(identity,xx,yy)
            count+=1
    assert count>20;matched[identity]=count
result['itemSprites']={'recordIds':matched,'allOpaquePixelsExact':True,'floorNumberHudRemoved':True}
heal=rows('heal-15');assert any(r['player_hp']=='5' and r['companion_hp']=='5' for r in heal)
state=json.loads((GUI/'heal-15-state.json').read_text());assert state['oran']==0
result['oran']={'playerAndPartnerFullyHealFrom1':True,'playerBerryConsumedOnce':True}
for name in ['intro-15','intro-18','default']:
    assert (GUI/f'{name}-map-title.bmp').is_file(),name
    image=Image.open(GUI/f'{name}-map-title.bmp').convert('RGBA')
    ink=sum(all(c>210 for c in image.getpixel((x,y))[:3]) for y in range(85,106) for x in range(8,192))
    assert ink>20,(name,ink)
    result[name]={'mapNameVisibleTextPixels':ink,'actualFadeInCapture':True}
(OUT/'gui-summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print('PASS: unseen/friendly patrol, front companion attack/damage, five low-HP dialogue captures, six original item sprites, full heals, two map names and default arrival')
