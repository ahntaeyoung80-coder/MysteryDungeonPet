"""Original guild dining cast and player room, exported as small native scene cues."""
import argparse
from copy import copy, deepcopy
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.mdpet.common import file_hash, write_json
from tools.mdpet.extractor import checked_pack, raw_wan_metadata
from tools.mdpet.packer import pack_asset, write_package
from tools.mdpet.wan_verify import verify_wan_asset
from tools.compile_runtime_package import compile_package
from tools.package_format import encode_package


def prepare(root, output):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.util import get_ppmdu_config_for_rom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.graphics.chara_wan.handler import CharaWanHandler
    from PIL import Image
    if output.exists():
        raise ValueError('choose a new output directory')
    rom_path = root / 'assets/original/source/user-supplied.nds'
    before = file_hash(rom_path)
    rom = NintendoDSRom.fromFile(str(rom_path)); config = get_ppmdu_config_for_rom(rom)
    byte_codec = copy(config); byte_codec.string_encoding = 'latin-1'
    # Preserve all string bytes for the opcode parser; never present this as decoded dialogue.
    refs = []
    def raw(path):
        data = rom.getFileByName(path)
        refs.append({'path': path, 'sha256': hashlib.sha256(data).hexdigest()})
        return data
    dining_source = 'SCRIPT/G01P06B/c00a0601'
    sleeping_source = 'SCRIPT/G01P07A/c00a0201'
    dining = FileType.SSA.deserialize(raw(dining_source+'.ssa'), config.script_data)
    sleeping = FileType.SSA.deserialize(raw(sleeping_source+'.ssa'), config.script_data)
    text = FileType.SSB.deserialize(raw(dining_source+'.ssb'), byte_codec).to_ssb_script()[0]
    wake_text = FileType.SSB.deserialize(raw(sleeping_source+'.ssb'), byte_codec).to_ssb_script()[0]
    if 'SetAnimation(18)' not in wake_text:
        raise ValueError('original wake alarm script changed')
    overlay = rom.loadArm9Overlays([11])[11]
    table_offset = 0x023218CC - overlay.ramAddress
    table = struct.unpack_from('<84H', overlay.data, table_offset)
    if table[42] != 0x080D or table[64] != 0x080F or table[76] != 0x081B:
        raise ValueError('original animation table address/edition mismatch')
    if table[18]&255 != 3: raise ValueError('original Loudred alarm clip changed')
    actor_codes, actor_effects = {}, {}
    actor = None
    for line in text.splitlines():
        match = re.search(r'(?:lives\(ACTOR_|def 1 for actor ACTOR_)(\w+)', line)
        if match:
            actor = match[1]
        match = re.search(r'SetAnimation\((\d+)\)', line)
        if actor and match: actor_codes[actor] = int(match[1])
        match = re.search(r'SetEffect\(EFFECT_(\w+),', line)
        if actor and match:
            actor_effects[actor] = next(e.id for e in config.script_data.sprite_effects if re.sub(r'(?<!^)(?=[A-Z])','_',e.name).upper()==match[1])
    def position(p):
        return [p.x_relative*8+(4 if p.x_offset>=2 else 0), p.y_relative*8+(4 if p.y_offset>=2 else 0)]
    rooms = [{'name':'GuildDining','source':dining_source+'.ssa','origin':[0,0],'size':[504,360],'actors':[]},
             {'name':'PlayerRoom','source':sleeping_source+'.ssa','origin':[0,0],'size':[384,312],'actors':[]}]
    for a in dining.layer_list[0].actors:
        code = actor_codes.get(a.actor.name)
        if code is None: raise ValueError(f'actor missing original meal clip: {a.actor.name}')
        p = position(a.pos)
        rooms[0]['actors'].append({'id':a.actor.id,'name':a.actor.name,'mdIndex':1 if a.actor.id==0 else 4 if a.actor.id==10 else a.actor.entid,
            'kind':0 if a.actor.id==0 else 1 if a.actor.id==10 else 2,'asset':'',
            'sourcePosition':p,'position':p,'direction':a.pos.direction.ssb_id,
            'scriptCode':code,'rawAnimationWord':table[code],'clip':table[code]&255,
            'effectId':actor_effects.get(a.actor.name,0)})
    for a in sleeping.layer_list[0].actors:
        if a.actor.id not in [0,10]: continue
        p = position(a.pos)
        rooms[1]['actors'].append({'id':a.actor.id,'name':a.actor.name,'mdIndex':1 if a.actor.id==0 else 4,
            'kind':0 if a.actor.id==0 else 1,'asset':'','sourcePosition':p,'position':p,
            'direction':a.pos.direction.ssb_id,'scriptCode':62,'rawAnimationWord':table[62],'clip':13,'effectId':0})
    if len(rooms[0]['actors']) != 12 or len(rooms[1]['actors']) != 2:
        raise ValueError('original guild cast/bed count mismatch')
    alarm = next(a for a in sleeping.layer_list[0].actors if a.actor.id==109)
    alarm_pos=position(alarm.pos)
    rooms[1]['actors'].append({'id':109,'name':alarm.actor.name,'mdIndex':alarm.actor.entid,
        'kind':2,'asset':'guild/actor-109/asset.mdpa','sourcePosition':alarm_pos,'position':alarm_pos,
        'direction':alarm.pos.direction.ssb_id,'scriptCode':18,'rawAnimationWord':table[18],'clip':3,'effectId':0})
    md = FileType.MD.deserialize(raw('BALANCE/monster.md'))
    packs = {path:checked_pack(raw(path)) for path in ('MONSTER/monster.bin','MONSTER/m_ground.bin','MONSTER/m_attack.bin')}
    with tempfile.TemporaryDirectory() as workspace:
        temp = Path(workspace); generated=temp/'guild';generated.mkdir()
        checks=[]
        for a in rooms[0]['actors']:
            if a['kind'] != 2: continue
            record=md.entries[a['mdIndex']]; sprite=record.sprite_index
            wans=[]; source_refs=[]
            for path,pack in packs.items():
                data=pack[sprite];source_refs.append({'sourcePath':path,'packSlot':sprite,'contentHash':hashlib.sha256(data).hexdigest()})
                if 'ground' not in path: data=FileType.PKDPX.deserialize(data).decompress()
                wans.append(CharaWanHandler.deserialize(data))
            originals=deepcopy(wans);merged=CharaWanHandler.merge_wan(*wans)
            sheets=temp/f'sheets-{a["id"]}';sheets.mkdir()
            CharaWanHandler.export_sheets(str(sheets),merged,config.animation_names.get(sprite,config.animation_names[0]))
            raw_meta=raw_wan_metadata(merged)
            names={int(n.findtext('Index')):n.findtext('Name') for n in ET.parse(sheets/'AnimData.xml').findall('Anims/Anim') if n.find('Index') is not None}
            clips={7,a['clip']} | ({0,3} if a['id']==109 else set())
            wanted={names[i]:i for i in clips if i in names}
            if clips != set(wanted.values()): raise ValueError(f'original guild actor clip missing: {a["name"]}')
            audit=temp/f'audit-{a["id"]}';audit.mkdir()
            source={'sourceKind':'rom','romHash':before,'edition':config.game_edition,'mdIndex':a['mdIndex'],'spriteIndex':sprite,
                'toolCommit':'5ed359a2560a1f04666b9ea02b71649b89cb16d7','sourceRefs':source_refs,'license':'game_assets_user_local_only','credit':'Nintendo / Pokémon / CHUNSOFT'}
            asset,report=pack_asset(sheets,audit,f'rom:guild:actor:{a["id"]}',source,raw_meta,required_animations=wanted,optional_animations={})
            verification=verify_wan_asset(merged,asset,audit,originals);report['romPixelMatch']='pass'
            checks.append({'actorId':a['id'],'name':a['name'],'mdIndex':a['mdIndex'],'spriteIndex':sprite,**verification})
            write_package(audit,[asset],[report])
            compiled=temp/f'compiled-{a["id"]}';compile_package(audit,compiled)
            dest=generated/f'actor-{a["id"]}';shutil.copytree(compiled/'asset-0',dest)
            a['asset']=f'guild/actor-{a["id"]}/asset.mdpa'
        for o in dining.layer_list[0].objects:
            path=f'GROUND/{o.object.name}.wan';data=raw(path);wan=FileType.WAN.deserialize(data)
            animation=wan.anim_groups[0][0]; images=[];frames=[]
            for frame in animation.frames:
                image,anchor=wan.render_frame(wan.frames[frame.frame_id]); images.append((image,anchor,frame))
            width=max(im.width for im,_,_ in images);height=sum(im.height for im,_,_ in images)
            atlas=Image.new('RGBA',(width,height));y=0
            for im,anchor,frame in images:
                atlas.paste(im,(0,y));frames.append({'rect':[0,y,im.width,im.height],'anchor':list(anchor),'shadow':[0,0],'ticks':max(1,frame.duration),'markers':[]});y+=im.height
            dest=generated/f'object-{o.object.id}';dest.mkdir();atlas.save(dest/'atlas.png')
            package={'assetVersion':1,'tickHz':60,'width':width,'height':height,'pngBytes':(dest/'atlas.png').stat().st_size,
                'sourceKind':1,'atlasName':'atlas.png','pngSha256':file_hash(dest/'atlas.png'),'assetKey':f'rom:guild:object:{o.object.id}',
                'provenance':json.dumps({'path':path,'sha256':hashlib.sha256(data).hexdigest()}),'clips':[{'id':0,'name':'DiningObject','rows':[frames]}]}
            (dest/'asset.mdpa').write_bytes(encode_package(package));(dest/'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original dining object\n')
            p=position(o.pos);rooms[0]['actors'].append({'id':10000+o.object.id,'name':o.object.name,'mdIndex':0,'kind':3,
                'asset':f'guild/object-{o.object.id}/asset.mdpa','sourcePosition':p,'position':p,
                'direction':0,'clip':0,'effectId':0})
        data=bytearray()
        def u32(value):data.extend(struct.pack('<I',value))
        def string(value):b=value.encode();u32(len(b));data.extend(b)
        u32(0x5347444D);u32(2);u32(2)
        for room in rooms:
            room['actors'].sort(key=lambda a:(a['position'][1],a['id']))
            string(room['name']);string(room['source']);u32(room['origin'][0]);u32(room['origin'][1]);u32(room['size'][0]);u32(room['size'][1]);u32(len(room['actors']))
            for a in room['actors']:
                if not all(0<=p<room['size'][axis] for axis,p in enumerate(a['position'])):raise ValueError('guild actor outside world')
                for n in [a['id'],a['mdIndex'],a['kind']]:u32(n)
                string(a['name']);string(a['asset'])
                for n in [*a['position'],a['direction'],a['clip'],a['effectId']]:u32(n)
        (generated/'guild.mdgs').write_bytes(data)
        report={'romHashUnchanged':file_hash(rom_path)==before,'romHash':before,'rooms':rooms,'sourceRefs':refs,'originalWANChecks':checks,
            'dialogue':'original unknown encoded strings preserved for opcode parsing; dialogue decoding unresolved',
            'effects':'AppleOnHead624 uses the original item109 apple sprite attached to Wigglytuff; Eating642/643 full effects remain unresolved',
            'wakeAlarm':{'actorId':109,'sourceScript':sleeping_source+'.ssb','scriptCode':18,'clip':3,'sourcePosition':alarm_pos,'durationMs':2000,'audio':'not extracted'},
            'sleepPolicy':'original bed positions/directions; EventSleep13 loop then Wake14; Laying27 source recorded',
            'mealPolicy':'original cast clips/loop flags; finite 8-second communal scene; meal consumption once'}
        if not report['romHashUnchanged']:raise ValueError('ROM changed')
        write_json(generated/'provenance.json',report)
        output.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(generated,output)
        return report

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();print(json.dumps(prepare(args.root,args.out),ensure_ascii=False,indent=2))
