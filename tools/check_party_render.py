"""Compare visible original body pixels against the first submitted Windows BMP."""
import argparse
import csv
import json
from pathlib import Path
import subprocess
from PIL import Image


def check(inspector, root, output):
    results = []
    for scenario, asset, tile_field in [('follow','asset-1','companion_tile'), ('attack','asset-2',None)]:
        directory = output/scenario
        with (directory/'trace.csv').open(newline='') as stream:
            row = next(r for r in csv.DictReader(stream) if r['submitted']=='1')
        package_path = root/'assets/original/gameplay/actors'/asset/'asset.mdpa'
        package = json.loads(subprocess.check_output([str(inspector),str(package_path)],text=True))
        # Fixtures start with an idle actor facing direction zero.
        clip = next(c for c in package['clips'] if c['id']==7)
        frame = clip['rows'][0][int(row['companion_frame']) if tile_field else 0]
        tile = int(row[tile_field]) if tile_field else 126
        screen_x = tile%24*24+12-int(row['camera_x'])
        screen_y = tile//24*24+12-int(row['camera_y'])
        atlas = Image.open(package_path.parent/'atlas.png').convert('RGBA')
        actual = Image.open(directory/'trace-first.bmp').convert('RGB')
        x,y,w,h = frame['rect']; ax,ay = frame['anchor']
        tested=matched=0
        for yy in range(h):
            for xx in range(w):
                expected=atlas.getpixel((x+xx,y+yy)); px=screen_x+xx-ax;py=screen_y+yy-ay
                if expected[3]!=255 or not (0<=px<200 and 0<=py<200):continue
                tested+=1;matched+=actual.getpixel((px,py))==expected[:3]
        if tested<20 or matched/tested<0.85:
            raise ValueError(f'{scenario}/{asset}: original body pixels {matched}/{tested}')
        results.append({'scenario':scenario,'asset':asset,'matchedPixels':matched,'testedPixels':tested,'status':'pass'})
    (output/'sprite-pixels.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(results))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--inspector',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();check(a.inspector,Path(__file__).resolve().parents[1],a.output)
