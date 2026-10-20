"""Check actual submitted player pixels while the partner shares their screen area."""
import argparse
import csv
import json
from pathlib import Path
import subprocess
from PIL import Image


def check(inspector, trace):
    root=Path(__file__).resolve().parents[1]
    with trace.open(newline='') as stream:
        row=next(r for r in csv.DictReader(stream) if r['submitted']=='1')
    path=root/'assets/original/gameplay/actors/asset-0/asset.mdpa'
    data=json.loads(subprocess.check_output([str(inspector),str(path)],text=True))
    clip=next(c for c in data['clips'] if c['id']==int(row['clip']))
    frame=clip['rows'][int(row['direction'])][int(row['frame_index'])]
    atlas=Image.open(path.parent/'atlas.png').convert('RGBA')
    actual=Image.open(trace.with_name(trace.stem+'-first.bmp')).convert('RGB')
    x,y,w,h=frame['rect'];ax,ay=frame['anchor']
    tested=matched=0
    partner_path=root/'assets/original/gameplay/actors/asset-1/asset.mdpa'
    partner_data=json.loads(subprocess.check_output([str(inspector),str(partner_path)],text=True))
    partner_clip=next(c for c in partner_data['clips'] if c['id']==int(row['partner_clip']))
    partner=partner_clip['rows'][6][int(row['partner_frame'])]
    partner_atlas=Image.open(partner_path.parent/'atlas.png').convert('RGBA')
    bx,by,bw,bh=partner['rect'];bax,bay=partner['anchor'];overlap=0
    for yy in range(h):
        for xx in range(w):
            color=atlas.getpixel((x+xx,y+yy))
            px=int(row['scene_x'])-int(row['camera_x'])+xx-ax
            py=int(row['scene_y'])-int(row['camera_y'])+yy-ay
            if color[3]!=255 or not(0<=px<200 and 0<=py<200):continue
            tested+=1;matched+=actual.getpixel((px,py))==color[:3]
            qx=px+int(row['camera_x'])-156+bax;qy=py+int(row['camera_y'])-184+bay
            if 0<=qx<bw and 0<=qy<bh and partner_atlas.getpixel((bx+qx,by+qy))[3]==255:overlap+=1
    if overlap<8:raise ValueError(f'overlap fixture missed partner body: {overlap} pixels')
    if tested<50 or matched/tested<0.99:raise ValueError(f'player overlap pixels {matched}/{tested}')
    result={'status':'pass','matchedPlayerPixels':matched,'testedPlayerPixels':tested,'overlappingBodyPixels':overlap}
    trace.with_suffix('.pixels.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--inspector',type=Path,required=True);parser.add_argument('--trace',type=Path,required=True)
    args=parser.parse_args();check(args.inspector,args.trace)
