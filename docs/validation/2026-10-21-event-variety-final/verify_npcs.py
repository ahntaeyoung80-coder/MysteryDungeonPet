"""실제 NPC EXE 캡처의 몸체·화자 얼굴·공유 캐시를 원작 선택 패키지와 대조한다."""
import csv,json,subprocess,sys
from pathlib import Path
from PIL import Image
ROOT=next(p for p in Path(__file__).resolve().parents if (p/'CMakeLists.txt').is_file())
SOURCE=ROOT/'build/event-variety-final/gui'
GAME=ROOT/'assets/original/gameplay'
characters={c['mdIndex']:c for c in json.loads((GAME/'npcs/catalogue.json').read_text())['characters']}
starters={c['mdIndex']:c for c in json.loads((GAME/'starters/catalogue.json').read_text())['characters']}
face_atlas=Image.open(GAME/'portraits/atlas.png').convert('RGBA')
face_audit=json.loads((GAME/'portraits/provenance.json').read_text())['portraits']
columns=sorted({p['bodyMdIndex'] for p in face_audit})
packages={}
def package(path):
    if path not in packages:
        packages[path]=json.loads(subprocess.check_output([str(ROOT/'build/p0/pet_package_inspect'),str(GAME/path)],text=True))
    return packages[path]
def frame(path,clip,direction,index):
    c=next(c for c in package(path)['clips'] if c['id']==int(clip))
    return c['rows'][int(direction) if len(c['rows'])>1 else 0][int(index)]
def sprite_rect(path,clip,direction,index,scene,camera):
    f=frame(path,clip,direction,index);x,y,w,h=f['rect']
    left=scene[0]-camera[0]-f['anchor'][0];top=scene[1]-camera[1]-f['anchor'][1]
    atlas=Image.open((GAME/path).parent/'atlas.png').convert('RGBA')
    return atlas.crop((x,y,x+w,y+h)),(left,top,left+w,top+h)
results={}
for md,c in characters.items():
    if len(sys.argv)>1 and md not in set(map(int,sys.argv[1:])):continue
    for relation in ('friendly','hostile'):
        name=f'npc-{md}-{relation}'
        with (SOURCE/(name+'.csv')).open() as f:rows=list(csv.DictReader(f))
        assert rows,name
        assert all(r['npc_cache_bodies']=='1' for r in rows),name
        assert all(r['npc_bindings']==f'1:{md}:{c["spriteIndex"]};2:{md}:{c["spriteIndex"]}' for r in rows),name
        first=next(r for r in rows if r['submitted']=='1')
        assert first['fade_opacity']=='0',name
        actors=[list(map(int,a.split(':'))) for a in first['actor_states'].split(';') if a]
        target=next(a for a in actors if a[0]==1)
        camera=(int(first['camera_x']),int(first['camera_y']))
        tile_point=lambda tile:(tile%24*24+12,tile//24*24+12)
        body,rect=sprite_rect(c['bodyManifest'],target[2],target[5],target[3],tile_point(target[1]),camera)
        blockers=[]
        for other in actors:
            if other[0]!=1:
                _,area=sprite_rect(c['bodyManifest'],other[2],other[5],other[3],tile_point(other[1]),camera);blockers.append(area)
        for path,clip,direction,index,x,y in [
            (starters[int(first['player_character'])]['bodyManifest'],first['clip'],first['direction'],first['frame_index'],int(first['scene_x']),int(first['scene_y'])),
            (starters[int(first['partner_character'])]['bodyManifest'],first['companion_clip'],0,first['companion_frame'],int(float(first['companion_world_x'])),int(float(first['companion_world_y'])))] :
            _,area=sprite_rect(path,clip,direction,index,(x,y),camera);blockers.append(area)
        if first['speaker']!='0':
            left=8 if first['speaker']=='1' else 136;blockers.append((left,88,left+56,144))
        image=Image.open(SOURCE/(name+'-first.bmp')).convert('RGBA')
        matched=0
        for y in range(body.height):
            for x in range(body.width):
                pixel=body.getpixel((x,y));sx=rect[0]+x;sy=rect[1]+y
                if pixel[3]!=255 or not(0<=sx<200 and 23<=sy<144):continue
                if any(l<=sx<r and t<=sy<b for l,t,r,b in blockers):continue
                assert image.getpixel((sx,sy))==pixel,(name,'body',x,y,image.getpixel((sx,sy)),pixel)
                matched+=1
        assert matched>5,(name,matched)
        result={'opaqueBodyPixels':matched,'rows':len(rows),'cacheBodies':1}
        if relation=='friendly':
            replies=[r for r in rows if r['speaker']=='3']
            assert replies and all(int(r['speaker_md'])==md for r in replies),name
            for r in replies:
                requested=int(r['emotion']);actual=requested if c['emotionMask']&(1<<requested) else 0
                assert int(r['actual_emotion'])==actual,(name,actual,r['actual_emotion'])
            mood=int(replies[0]['actual_emotion']);x=columns.index(md)*40
            expected=face_atlas.crop((x,mood*40,x+40,mood*40+40))
            reply=Image.open(SOURCE/(name+'-npc-reply.bmp')).convert('RGBA')
            assert reply.crop((144,96,184,136)).tobytes()==expected.tobytes(),(name,'portrait')
            result['portraitMd']=md;result['actualEmotion']=mood
        results[name]=result
(SOURCE.parent/'npc-gui-summary.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
print(f'PASS: {len(results)} actual NPC bodies, {sum("portraitMd" in r for r in results.values())} speaker faces, requested/actual emotions and one shared body for two instances')
