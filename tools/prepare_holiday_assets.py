"""검증된 ground·원작 Float20·앞/뒤 물결 조각을 휴일 선택 자산으로 추출한다."""
import argparse
from copy import copy
import hashlib
import json
from pathlib import Path
import struct
from collections import deque
from PIL import Image
from tools.package_format import encode_package
from tools.prepare_party_actor import prepare as prepare_body
from tools.mdpet.extractor import checked_pack
from tools.prepare_dialogue_portraits import body_identity


def prepare(root, output):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.graphics.effect_wan.handler import EffectWanHandler
    from skytemple_files.graphics.effect_wan.sheets import GenerateFrame, GetPieceRect
    if output.exists(): raise ValueError('choose a new holiday output directory')
    digest=lambda b:hashlib.sha256(b).hexdigest()
    source=root/'assets/original/source/user-supplied.nds';before=digest(source.read_bytes());rom=NintendoDSRom.fromFile(str(source))
    evidence=root/'docs/research/2026-10-21-event-resources'
    backgrounds=json.loads((evidence/'backgrounds.json').read_text())
    output.mkdir(parents=True);proof=[]
    def package(path,key,atlas,clips,provenance):
        atlas.save(output/(path+'.png'));png=(output/(path+'.png')).read_bytes()
        record={'assetVersion':1,'tickHz':60,'width':atlas.width,'height':atlas.height,'pngBytes':len(png),'sourceKind':1,'atlasName':path+'.png','pngSha256':digest(png),'assetKey':key,'provenance':json.dumps(provenance,sort_keys=True),'clips':clips}
        (output/(path+'.mdpa')).write_bytes(encode_package(record))
    for row in backgrounds['backgrounds']:
        level=row['level']
        if level not in {'P01P01A','T01P01A','T01P02A','P02P01A'}:continue
        for ref in row['backgroundSources']:
            if digest(rom.getFileByName(ref['path']))!=ref['sha256']:raise ValueError('ground source changed')
        image=Image.open(evidence/row['image']['file']).convert('RGBA')
        if digest(image.tobytes())!=row['image']['rgbaSha256']:raise ValueError('ground evidence pixels changed')
        provenance={'romHash':before,'ground':level,'sources':row['backgroundSources'],'rgbaSha256':digest(image.tobytes()),'scope':'original static BMA/BPC/BPL/BPA image; product visit paths are separate'}
        package(level,'rom:EoS_NA:holiday:'+level,image,[{'id':0,'name':level,'rows':[[{'rect':[0,0,*image.size],'anchor':[0,0],'shadow':[0,0],'ticks':1,'markers':[]}]]}],provenance)
        proof.append(provenance)
    raw=checked_pack(rom.getFileByName('EFFECT/effect.bin'))[1];effect=EffectWanHandler.deserialize(raw)
    images=[];clips=[];water=[]
    def signed(value):return value-65536 if value>=32768 else value
    for effect_id in range(625,630):
        animation=effect_id-609;frames=effect.animData[animation];parts=[];boxes=[]
        for frame in frames:
            pieces=[]
            for original in effect.frameData[frame.frmIndex]:
                piece=copy(original);piece.offset=(piece.offset[0]+signed(frame.offset[0]),piece.offset[1]+signed(frame.offset[1]));pieces.append(piece)
                if piece.paletteIndex!=1:raise ValueError('original yellow-water palette changed')
                box=GetPieceRect(effect,piece)
                if box:boxes.append(box)
            parts.append(pieces)
        box=(min(b[0] for b in boxes),min(b[1] for b in boxes),max(b[2] for b in boxes),max(b[3] for b in boxes))
        for front in (False,True):
            rows=[]
            for frame,pieces in zip(frames,parts):
                image=GenerateFrame(effect,[p for p in pieces if p.front==front],box)
                if image is None:image=Image.new('RGBA',(box[2]-box[0],box[3]-box[1]))
                images.append(image);rows.append({'image':len(images)-1,'anchor':[256-box[0],512-box[1]],'ticks':int(frame.duration)})
            clips.append({'id':effect_id*2+int(front),'name':f'Water{effect_id}'+('Front' if front else 'Back'),'rows':rows})
        water.append({'effectId':effect_id,'animation':animation,'frontBack':True,'palette':1,'box':box,'anchor':[256-box[0],512-box[1]],'ticks':[int(f.duration) for f in frames]})
    atlas=Image.new('RGBA',(max(i.width for i in images),sum(i.height for i in images)));positions=[];y=0
    for image in images:atlas.paste(image,(0,y));positions.append([0,y,*image.size]);y+=image.height
    packed=[]
    for clip in clips:
        packed.append({'id':clip['id'],'name':clip['name'],'rows':[[{'rect':positions[f['image']],'anchor':f['anchor'],'shadow':[0,0],'ticks':f['ticks'],'markers':[]} for f in clip['rows']]]})
    package('water','rom:EoS_NA:holiday:water625-629',atlas,packed,{'romHash':before,'source':'EFFECT/effect.bin:1','wanSha256':digest(raw),'effects':water,'priority':'original MetaFrame.front flag; back -> body -> front'})
    starters=json.loads((root/'assets/original/gameplay/starters/catalogue.json').read_text());md=FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md'))
    bindings=[];prepared={};frames=0
    for character in starters['characters']:
        index=character['mdIndex'];sprite=int(md.entries[index].sprite_index)
        if sprite not in prepared:
            report=prepare_body(root,output/'float'/str(sprite),index,clip_ids=(20,));prepared[sprite]=report;frames+=report['comparedFrames']
        _,key,body_hash,_=body_identity(output/'float'/str(sprite)/'asset.mdpa')
        bindings.append({'mdIndex':index,'spriteIndex':sprite,'manifest':f'float/{sprite}/asset.mdpa','bodyKey':key,'bodyHash':body_hash,'verification':prepared[sprite]})
    data=bytearray(struct.pack('<3I',0x5348444D,2,len(bindings)))
    for b in bindings:
        data.extend(struct.pack('<2I',b['mdIndex'],b['spriteIndex']))
        for text in (b['manifest'],b['bodyKey'],b['bodyHash']):
            rawpath=text.encode();data.extend(struct.pack('<I',len(rawpath)));data.extend(rawpath)
    (output/'float.mdhs').write_bytes(data)
    (output/'provenance.json').write_text(json.dumps({'romHash':before,'grounds':proof,'water':water,'floatBindings':bindings,'floatComparedFrames':frames,'policy':'static grounds; original Float/ticks/anchors/front-back priority; no SteamClouds43'},ensure_ascii=False,indent=2)+'\n')
    (output/'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original ROM ground, Float and yellow-water effect\n')
    if before!=digest(source.read_bytes()):raise ValueError('ROM changed')
    compile_routes(root,output,rom)
    return {'grounds':len(proof),'floatCharacters':len(bindings),'floatBodies':len(prepared),'floatFrames':frames,'waterEffects':len(water),'romUnchanged':True}


def compile_routes(root,output,rom=None):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    if rom is None:rom=NintendoDSRom.fromFile(str(root/'assets/original/source/user-supplied.nds'))
    selected=json.loads((root/'assets/original/gameplay/holiday-visits.json').read_text())['routes']
    data=bytearray(struct.pack('<3I',0x5248444D,1,len(selected)));proof=[]
    def u32(v):data.extend(struct.pack('<I',v))
    for route in selected:
        bma=FileType.BMA.deserialize(rom.getFileByName('MAP_BG/'+route['scene'].lower()+'.bma'));w,h=bma.map_width_camera,bma.map_height_camera
        u32(route['id']);name=route['scene'].encode();u32(len(name));data.extend(name);u32(w*8);u32(h*8)
        paths=[]
        for role in ('player','partner'):
            points=[(x//8,y//8) for x,y in route[role]];path=[points[0]]
            for start,end in zip(points,points[1:]):
                for x,y in (start,end):
                    if not 0<=x<w or not 0<=y<h or bma.collision[y*w+x]:raise ValueError(f'blocked visit waypoint {route["scene"]}/{role}: {x*8},{y*8}')
                parents={start:None};queue=deque([start])
                while queue and end not in parents:
                    x,y=queue.popleft()
                    for dx,dy in ((0,-1),(1,0),(0,1),(-1,0)):
                        nxt=(x+dx,y+dy)
                        if 0<=nxt[0]<w and 0<=nxt[1]<h and not bma.collision[nxt[1]*w+nxt[0]] and nxt not in parents:parents[nxt]=(x,y);queue.append(nxt)
                if end not in parents:raise ValueError('visit route disconnected')
                part=[];at=end
                while at!=start:part.append(at);at=parents[at]
                path.extend(reversed(part))
            pixels=[[x*8+4,y*8+4] for x,y in path];paths.append(pixels);u32(len(pixels))
            for x,y in pixels:u32(x);u32(y)
        proof.append(dict(route,compiledPlayer=paths[0],compiledPartner=paths[1],collision='every original BMA 8px cell pass'))
    (output/'routes.mdhr').write_bytes(data)
    (output/'routes-proof.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2)+'\n')
    return {'routes':len(proof),'collision':'pass'}


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path('.'));p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(json.dumps(prepare(a.root,a.out)))
