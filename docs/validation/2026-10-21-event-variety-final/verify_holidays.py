"""실제 입욕 EXE DIB를 원작 ground/Float/앞뒤 물결/고정 배우로 독립 합성해 대조한다."""
import csv,json,struct,subprocess
from pathlib import Path
from PIL import Image
ROOT=next(p for p in Path(__file__).resolve().parents if (p/'CMakeLists.txt').is_file())
GAME=ROOT/'assets/original/gameplay'
OUT=ROOT/'build/event-variety-final'
packages={}
starters={c['mdIndex']:c for c in json.loads((GAME/'starters/catalogue.json').read_text())['characters']}
floats={c['mdIndex']:c for c in json.loads((GAME/'holiday/provenance.json').read_text())['floatBindings']}
npcs={c['mdIndex']:c for c in json.loads((GAME/'npcs/catalogue.json').read_text())['characters']}
def package(path):
    if path not in packages:packages[path]=json.loads(subprocess.check_output([str(ROOT/'build/p0/pet_package_inspect'),str(path)],text=True))
    return packages[path]
def draw(canvas,path,clip,direction,elapsed,point):
    p=package(path);c=next(c for c in p['clips'] if c['id']==clip);row=c['rows'][direction if len(c['rows'])>1 else 0]
    tick=(elapsed*p['tickHz']//1000)%sum(f['ticks'] for f in row)
    for f in row:
        if tick<f['ticks']:break
        tick-=f['ticks']
    atlas=Image.open(path.parent/('water.png' if path.name=='water.mdpa' else 'atlas.png')).convert('RGBA')
    x,y,w,h=f['rect'];sprite=atlas.crop((x,y,x+w,y+h));canvas.alpha_composite(sprite,(point[0]-184-f['anchor'][0],point[1]-100-f['anchor'][1]))
results={}
for profile in ('default','meowth-munchlax','munchlax-meowth','female'):
    name='bath-'+profile
    rows=list(csv.DictReader((OUT/'gui'/f'{name}.csv').open()));r=next(r for r in rows if r['submitted']=='1')
    assert r['holiday_beat']=='14' and r['bathing']=='1' and r['holiday_clip']=='20' and r['holiday_fade']=='0'
    raw=(OUT/'fixtures'/f'{name}.mdes').read_bytes();length=struct.unpack_from('<I',raw,len(raw)-8)[0];start=len(raw)-8-length
    started=struct.unpack_from('<Q',raw,start+32)[0];elapsed=max(0,int(r['utc_ms'])-started)
    player=(int(r['holiday_player_x']),int(r['holiday_player_y']));partner=(int(r['holiday_partner_x']),int(r['holiday_partner_y']))
    canvas=Image.open(GAME/'holiday/P02P01A.png').convert('RGBA').crop((184,100,384,300))
    draw(canvas,GAME/npcs[352]['bodyManifest'],7,7,elapsed,(316,132))
    for point in (player,partner):draw(canvas,GAME/'holiday/water.mdpa',1250,0,elapsed,point)
    for role,point in (('player',player),('partner',partner)):
        md=int(r[role+'_character']);direction=int(r['holiday_'+role+'_direction'])
        draw(canvas,GAME/'holiday'/floats[md]['manifest'],20,direction,elapsed,point)
    for point in (player,partner):draw(canvas,GAME/'holiday/water.mdpa',1251,0,elapsed,point)
    actual=Image.open(OUT/'gui'/f'{name}-first.bmp').convert('RGBA')
    expected=canvas.tobytes();observed=actual.tobytes();matched=sum(expected[i:i+4]==observed[i:i+4] for i in range(0,len(expected),4))
    assert expected==observed,(name,matched,elapsed)
    results[name]={'matchedPixels':matched,'playerMd':int(r['player_character']),'partnerMd':int(r['partner_character']),'floatClip':20,'layerOrder':'original ground/NPC/back water/Float bodies/front water','hudAbsent':True}
(OUT/'holiday-pixel-summary.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
print('PASS: four actual bath combinations, 160,000 exact submitted pixels, original occlusion/order and no HUD')
