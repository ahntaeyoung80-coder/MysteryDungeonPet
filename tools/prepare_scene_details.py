"""원작 AppleOnHead624 효과와 대화창 frame_b0 타일을 MDPA로 추출한다."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.package_format import encode_package
from tools.mdpet.extractor import checked_pack


def prepare(root, output):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.graphics.effect_wan.handler import EffectWanHandler
    from skytemple_files.graphics.effect_wan.sheets import GetPieceRect, GenerateFrame
    from PIL import Image
    if output.exists(): raise ValueError('choose a new output directory')
    source=root/'assets/original/source/user-supplied.nds'
    original=source.read_bytes();rom= NintendoDSRom(original)
    digest=lambda b:hashlib.sha256(b).hexdigest()
    rom_hash=digest(original)
    effects=rom.getFileByName('EFFECT/effect.bin');ov=rom.loadArm9Overlays([10])[10]
    row=bytes(ov.data[0x022CC52C-ov.ramAddress+624*28:0x022CC52C-ov.ramAddress+625*28])
    file_type,slot,palette,animation,se,unused=struct.unpack_from('<6I',row)
    if (file_type,slot,palette,animation,tuple(row[24:]))!=(2,1,1,15,(0,0,0,1)):
        raise ValueError('AppleOnHead624 source mapping changed')
    raw=checked_pack(effects)[slot];wan=EffectWanHandler.deserialize(raw)
    sequence=wan.animData[animation]
    bounds=[GetPieceRect(wan,m) for f in sequence for m in wan.frameData[f.frmIndex]]
    box=(min(b[0] for b in bounds),min(b[1] for b in bounds),max(b[2] for b in bounds),max(b[3] for b in bounds))
    width,height=box[2]-box[0],box[3]-box[1]
    atlas=Image.new('RGBA',(width,height*len(sequence)));frames=[];audit=[]
    s16=lambda n:n-65536 if n>=32768 else n
    for i,f in enumerate(sequence):
        image=GenerateFrame(wan,wan.frameData[f.frmIndex],box)
        if image is None:raise ValueError('missing original effect frame')
        atlas.paste(image,(0,i*height))
        offset=[s16(v) for v in f.offset]
        frame={'rect':[0,i*height,width,height],
            'anchor':[256-box[0]-offset[0],508-box[1]-offset[1]],'shadow':[0,0],
            'ticks':f.duration,'markers':[],'rawOffset':offset,
            'sourceMetadata':json.dumps({'sourceFrame':f.frmIndex,'sequenceOffset':offset,'palette':palette})}
        frames.append(frame);audit.append({'frame':f.frmIndex,'ticks':f.duration,'offset':offset,
            'rgbaSha256':digest(image.tobytes()),'front':all(m.front for m in wan.frameData[f.frmIndex])})
    def write(folder,atlas,clips,key,provenance):
        folder.mkdir(parents=True);atlas.save(folder/'atlas.png')
        png=(folder/'atlas.png').read_bytes()
        package={'assetVersion':1,'tickHz':60,'width':atlas.width,'height':atlas.height,'pngBytes':len(png),
            'sourceKind':1,'atlasName':'atlas.png','pngSha256':digest(png),'assetKey':key,
            'provenance':json.dumps(provenance,sort_keys=True),'clips':clips}
        (folder/('apple.mdpa' if key.endswith('624') else 'ui.mdpa')).write_bytes(encode_package(package))
        (folder/'provenance.json').write_text(json.dumps(provenance,ensure_ascii=False,indent=2)+'\n')
        (folder/'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original ROM graphics\n')
    apple={'romSha256':rom_hash,'sourcePath':'EFFECT/effect.bin','sourceSha256':digest(effects),
        'overlayId':10,'effectTableAddress':'0x022CC52C','effectId':624,'entryHex':row.hex(),
        'fileType':file_type,'packSlot':slot,'wanSha256':digest(raw),'palette':palette,'animation':animation,
        'wanOffset':'Head (0), original script marker argument 3 retained as source context',
        'loop':bool(row[27]),'tickHz':60,'frames':audit,
        'verification':'direct effect-WAN GeneratePiece/DrawOn pixels, signed sequence offsets, hFlip and durations retained; game wall time not measured'}
    write(output/'effects',atlas,[{'id':624,'name':'AppleOnHead','rows':[frames]}],'rom:effect:624',apple)
    raw_chr=rom.getFileByName('FONT/frame_b0.chr');raw_pal=rom.getFileByName('FONT/frame_b0.pal')
    chr_=FileType.CHR.deserialize(raw_chr);chr_.set_palette(FileType.PAL.deserialize(raw_pal))
    indexed=chr_.to_pil();panel=indexed.convert('RGBA')
    panel.putalpha(Image.frombytes('L',panel.size,bytes(0 if c==0 else 255 for c in indexed.tobytes())))
    tileframes=[{'rect':[i*8,0,8,8],'anchor':[0,0],'shadow':[0,0],'ticks':1,'markers':[]} for i in range(18)]
    ui={'romSha256':rom_hash,'chrPath':'FONT/frame_b0.chr','chrSha256':digest(raw_chr),
        'palPath':'FONT/frame_b0.pal','palSha256':digest(raw_pal),'rgbaSha256':digest(panel.tobytes()),
        'nineSliceTiles':[[2,3,4],[5,1,6],[7,8,9]],'tileSize':8,
        'originalDialogueParamsHex':bytes(rom.arm9[0x9AF2C:0x9AF3C]).hex(),
        'layout':'original 8px border tiles; 184x48 adaptation for fixed 200px viewport; left aligned text',
        'font':'Korean UI text uses Windows Malgun Gothic at12px; original glyph decoding is not claimed'}
    write(output/'ui',panel,[{'id':0,'name':'OriginalWindowTiles','rows':[tileframes]}],'rom:font:frame-b0',ui)
    if digest(source.read_bytes())!=rom_hash:raise ValueError('ROM changed')
    return {'romUnchanged':True,'appleFrames':len(frames),'totalTicks':sum(f.duration for f in sequence),
        'appleOffsets':sorted(set(tuple(v['offset']) for v in audit)),'originalWindowTiles':18}


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();print(json.dumps(prepare(a.root,a.out),ensure_ascii=False,indent=2))
