"""Render inspection previews from the exact MDPA frame metadata used by native playback."""
import argparse
import json
from pathlib import Path
import subprocess
from PIL import Image


def preview(assets, inspector, output):
    cache = {}
    def load(path):
        if path not in cache:
            data = json.loads(subprocess.check_output([str(inspector),str(path)],text=True))
            cache[path] = (data,Image.open(path.parent/bytes.fromhex(data['atlasNameHex']).decode()).convert('RGBA'))
        return cache[path]
    def sample(path, clip_id, direction, ms, hold=False):
        data,atlas = load(path)
        clip=next(c for c in data['clips'] if c['id']==clip_id)
        frames=clip['rows'][0 if len(clip['rows'])==1 else direction]
        tick=(ms*data['tickHz']//1000)*clip['speedNumerator']//clip['speedDenominator']
        tick=min(tick,sum(f['ticks'] for f in frames)-1) if hold else tick%sum(f['ticks'] for f in frames)
        for f in frames:
            if tick<f['ticks']:break
            tick-=f['ticks']
        x,y,w,h=f['rect']
        return atlas.crop((x,y,x+w,y+h)), f
    rooms=json.loads((assets/'guild/provenance.json').read_text())['rooms']
    output.mkdir(parents=True,exist_ok=True)
    for index,name,bg,alarm in [(0,"guild-dining",0,False),(1,"player-room-sleep",2,False),(1,"loudred-alarm",1,True)]:
        room=rooms[index]
        images=[]
        for ms in range(0,3600 if alarm else 2000,50):
            frame,_=sample(assets/'home/home.mdpa',bg,0,ms)
            # The same final player layer used by the production compositor.
            actors=sorted(room['actors'],key=lambda actor:actor['kind']==0)
            for actor in actors:
                if index==1 and actor['id']==109 and not alarm: continue
                path = assets/('actors/asset-0/asset.mdpa' if actor['kind']==0 else 'actors/asset-1/asset.mdpa' if actor['kind']==1 else actor['asset'])
                direction=actor['direction'];position=list(actor['position']);clip=actor['clip'];elapsed=ms
                hold=index==1 and actor['kind'] in (0,1)
                if hold:clip=13;elapsed=60000
                if index==0 and actor['id']==79:direction=0 if (ms*60//1000//20)%2 else 4
                if index==1 and actor['id']==109:
                    if ms<800:position[0]-=48-48*ms//800;clip=0;direction=2
                    elif ms<2800:clip=3;direction=2;elapsed=ms-800
                    else:position[0]-=48*(ms-2800)//800;clip=0;direction=6;elapsed=ms-2800
                image,metadata=sample(path,clip,direction,elapsed,hold)
                anchor=metadata['anchor']
                frame.alpha_composite(image,(position[0]-anchor[0],position[1]-anchor[1]))
                if index==0 and actor['id']==79:
                    apple,apple_metadata=sample(assets/'effects/apple.mdpa',624,0,ms)
                    head=next(m['point'] for m in metadata['markers'] if m['kind']==0)
                    apple_anchor=apple_metadata['anchor']
                    frame.alpha_composite(apple,(position[0]+head[0]-anchor[0]-apple_anchor[0],position[1]+head[1]-anchor[1]-apple_anchor[1]))
            images.append(frame)
        images[0].save(output/(name+'.gif'),save_all=True,append_images=images[1:],duration=50,loop=0,disposal=2)
        images[0].save(output/(name+'.png'))
    return {'source':'native MDPA inspector frame/anchor/tick metadata','cast':12,'roomSleepers':2,
        'wakeAlarm':'original Loudred Walk0 in800ms / alarm3 2000ms / Walk0 out800ms; no original audio',
        'effectGraphics':'original AppleOnHead624, effect.bin slot1 animation15,13 frames,52 ticks, head marker attachment'}


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--assets',type=Path,required=True);p.add_argument('--inspector',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();print(json.dumps(preview(a.assets,a.inspector,a.out),ensure_ascii=False))
