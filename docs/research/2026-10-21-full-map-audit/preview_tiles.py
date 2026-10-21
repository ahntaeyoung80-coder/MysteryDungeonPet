"""ROM 정규 타일셋 전체의 정적 팔레트/청크를 대조하고 문서용 표본을 만든다."""
from pathlib import Path
import json,hashlib,sys,time
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[3];OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from ndspy.rom import NintendoDSRom
from skytemple_files.common.types.file_types import FileType
from skytemple_files.common.util import get_ppmdu_config_for_rom
from skytemple_files.common.impl_cfg import change_implementation_type,ImplementationType
start=time.monotonic();change_implementation_type(ImplementationType.PYTHON)
source=ROOT/'assets/original/source/user-supplied.nds';before=hashlib.sha256(source.read_bytes()).hexdigest()
rom=NintendoDSRom.fromFile(str(source));config=get_ppmdu_config_for_rom(rom)
pack=FileType.DUNGEON_BIN.deserialize(rom.getFileByName('DUNGEON/dungeon.bin'),static_data=config)
summary=json.loads((OUT/'summary.json').read_text());ids=[i for i in summary['tileIds'] if i<170]
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',11)
small=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',10)
samples={};records=[]
for identity in ids:
    try:
        dma,dpc,dpci,dpl=(pack[offset+identity] for offset in (170,340,510,680))
        strip=dpc.chunks_to_pil(dpci,dpl.palettes,1);count=len(dpc.chunks)
        assert strip.size==(24,count*24)
        mappings=[int(v) for t in range(3) for n in range(256) for v in dma.get(t,n)]
        assert len(mappings)==2304 and all(0<=i<count for i in mappings)
        keys=list(dict.fromkeys(int(v) for t in range(3) for n in (255,0,15,240,85,170) for v in dma.get(t,n)))
        keys+=list(range(count));keys=list(dict.fromkeys(keys))[:16]
        card=Image.new('RGB',(96,96),(32,32,36))
        for i,chunk in enumerate(keys):
            indexed=strip.crop((0,chunk*24,24,chunk*24+24));rgba=indexed.convert('RGBA');rgba.putalpha(Image.frombytes('L',indexed.size,bytes(0 if p%16==0 else 255 for p in indexed.tobytes())))
            card.paste(rgba,((i%4)*24,(i//4)*24),rgba)
        samples[identity]=card
        records.append({'tileset':identity,'chunks':count,'rgbaSha256':hashlib.sha256(strip.convert('RGBA').tobytes()).hexdigest(),'dmaValid':True})
    except Exception as e:records.append({'tileset':identity,'error':str(e)})
sheet=Image.new('RGB',(1000,118*((len(ids)+9)//10)),(18,20,26));draw=ImageDraw.Draw(sheet)
for index,identity in enumerate(ids):
    x=(index%10)*100;y=(index//10)*118
    if identity in samples:sheet.paste(samples[identity],(x+2,y+2))
    draw.text((x+3,y+101),f'Tileset {identity}',font=small,fill=(240,240,240))
sheet.save(OUT/'all-regular-tilesets.png')
groups=json.loads((OUT/'route-groups.json').read_text());groups=[g for g in groups if g['category']!='dojo']
sheet=Image.new('RGB',(1120,124*((len(groups)+4)//5)),(18,20,26));draw=ImageDraw.Draw(sheet)
for index,g in enumerate(groups):
    x=(index%5)*224;y=(index//5)*124;identity=next((i for i in g['tilesets'] if i in samples),None)
    if identity is not None:sheet.paste(samples[identity],(x+4,y+4))
    lines=['']
    for word in g['name'].split():
        if len(lines[-1])+len(word)>17:lines.append(word)
        else:lines[-1]+=(' ' if lines[-1] else '')+word
    draw.text((x+106,y+8),f"G{g['group']:02d}",font=font,fill=(255,205,90))
    for j,line in enumerate(lines):draw.text((x+106,y+28+j*14),line,font=font,fill=(240,240,240))
    values=g['dungeonIds'];parts=[]
    for value in values:
        if not parts or value!=parts[-1][-1]+1:parts.append([value])
        else:parts[-1].append(value)
    label=','.join(str(p[0]) if len(p)==1 else f'{p[0]}-{p[-1]}' for p in parts)
    draw.text((x+5,y+105),f"IDs {label}",font=small,fill=(150,190,220))
sheet.save(OUT/'all-named-route-samples.png')
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
record={'source':'DUNGEON/dungeon.bin DMA/DPC/DPCI/DPL','scope':'정적 원작 청크 표본; 실제 던전 생성/팔레트 애니메이션/보스 BGP 배경은 아님','seconds':time.monotonic()-start,'romUnchanged':True,'regularTilesets':len(ids),'converted':sum('error' not in r for r in records),'fixedBackgroundTileIds':[i for i in summary['tileIds'] if i>=170],'tilesets':records}
(OUT/'tile-validation.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');print(json.dumps({k:v for k,v in record.items() if k!='tilesets'},ensure_ascii=False))
