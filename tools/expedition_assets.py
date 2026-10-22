"""선택 원정의 원작 타일/연결부/문맥을 기존 MDPA/MDPE 계약으로 준비한다."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
from tools.prepare_expeditions import catalogue, write_plan, Writer, AUDIT, ROUTES
from tools.mdpet.b1 import export_tileset, write_profiles
from tools.mdpet.common import canonical
from tools.mdpet.catalogue import cdf_weights
from tools.package_format import encode_package
from collections import deque


def fixed_layouts(plans, rom):
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.dungeon_data.fixed_bin.model import TileRule, EntityRule, FloorType, TileRuleType
    rooms=FileType.FIXED_BIN.deserialize(rom.getFileByName('BALANCE/fixed.bin'))
    for plan in plans:
        for f in plan['floors']:
            if not f['fixedFloorId']:continue
            room=rooms.fixed_floors[f['fixedFloorId']];tiles=set();start=None
            for i,a in enumerate(room.actions):
                x,y=i%room.width,i//room.width
                floor=isinstance(a,EntityRule) or (isinstance(a,TileRule) and a.tr_type.floor_type==FloorType.FLOOR)
                if floor and x<22 and y<22:tiles.add((y+1)*24+x+1)
                if isinstance(a,TileRule) and a.tr_type==TileRuleType.LEADER_SPAWN:start=(y+1)*24+x+1
            if start not in tiles:raise ValueError('fixed leader outside pet viewport geometry')
            distance={start:0};queue=deque([start])
            while queue:
                t=queue.popleft()
                for n in (t-24,t+24,t-1,t+1):
                    if n in tiles and n not in distance:distance[n]=distance[t]+1;queue.append(n)
            for s in f['fixedSpawns']+f.get('revisitSpawns',[]):
                if (s['mdIndex'] or s['itemId']) and (s['y']+1)*24+s['x']+1 not in distance:
                    raise ValueError('fixed actor/item is outside connected selected geometry')
            occupied={(s['y']+1)*24+s['x']+1 for s in f['fixedSpawns']+f.get('revisitSpawns',[])}
            end=max((t for t in distance if t not in occupied),key=lambda t:(distance[t],t))
            f.update(fixedTiles=sorted(distance),fixedStart=start,fixedExit=end,
                     backgroundX=((32-room.width)//2-1)*24,backgroundY=((32-room.height)//2-1)*24)


def export_fixed_background(pack, identity, output, rom_hash):
    index=identity-170
    if not 0<=index<29:raise ValueError('unsupported fixed background ID')
    pic=pack[937+index].to_pil(pack[879+index],pack[908+index],pack[966+index].palettes).convert('RGBA')
    pic.putalpha(255);stem=f'tileset-{identity}';pic.save(output/(stem+'.png'))
    provenance={'sourceKind':'rom','romHash':rom_hash,'tilesetId':identity,
                'slots':[850+index,879+index,908+index,937+index,966+index],
                'scope':'original static DBG/DPC/DPCI/DPL background; palette animation pending'}
    package={'assetVersion':1,'tickHz':60,'width':pic.width,'height':pic.height,
             'pngBytes':(output/(stem+'.png')).stat().st_size,'sourceKind':1,'atlasName':stem+'.png',
             'pngSha256':hashlib.sha256((output/(stem+'.png')).read_bytes()).hexdigest(),
             'assetKey':f'rom:fixed-background:{identity}','provenance':json.dumps(provenance,sort_keys=True),
             'clips':[{'id':0,'name':'FixedBackground','rows':[[{'rect':[0,0,pic.width,pic.height],'anchor':[0,0],'shadow':[0,0],'ticks':1,'markers':[]}]]}]}
    (output/(stem+'.mdpa')).write_bytes(encode_package(package))
    return {'id':identity,'count':1,'package':stem+'.mdpa','mappings':[0]*2304,'provenance':provenance}


def branch_plans(root, data):
    import gzip
    physical = {p['physicalId']: p for p in map(json.loads, gzip.open(root/AUDIT/'gameplay-floor-profiles.jsonl.gz', 'rt'))}
    maps = {m['dungeonId']: m for m in json.loads((root/AUDIT/'map-gameplay.json').read_text())}
    result = []
    for b in data['branches']:
        m = maps[b['viaDungeonId']]
        rows = []
        for f in m['floorRefs']:
            p = physical[f['physicalId']]
            if f['fixedFloorId']: raise ValueError('branch fixed-room policy has not been selected')
            rows.append({'dungeonId': m['dungeonId'], 'localFloor': f['localFloor0'], 'name': m['name'],
                         'physicalId': f['physicalId'], 'fixedFloorId': 0, 'fixedSpawns': [],
                         'profile': dict(p, key={'edition': p['edition'], 'dungeonId': m['dungeonId'], 'localFloor': f['localFloor0']}),
                         'exitLink': '', 'groundIds': []})
        result.append({'id': f"RUN-D{m['dungeonId']:03d}", 'name': m['name'], 'dungeonIds': [m['dungeonId']], 'entryGroundIds': [], 'floors': rows})
    return result


def ground_assets(root, output, data, rom, config, pack):
    from PIL import Image
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.util import get_binary_from_rom
    from skytemple_files.hardcoded.ground_dungeon_tilesets import HardcodedGroundDungeonTilesets, resolve_mapping_for_level
    from skytemple_files.hardcoded.dungeons import HardcodedDungeons
    from skytemple_files.graphics.dma.dma_drawer import DmaDrawer
    from skytemple_files.graphics.chara_wan.handler import CharaWanHandler
    from tools.mdpet.extractor import checked_pack
    from tools.mdpet.wan_verify import render_wan_frame
    mapping = HardcodedGroundDungeonTilesets.get_ground_dungeon_tilesets(get_binary_from_rom(rom,config.bin_sections.overlay11),config)
    definitions = HardcodedDungeons.get_dungeon_list(get_binary_from_rom(rom,config.bin_sections.arm9),config)
    mappa = FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'))
    fixed = FileType.FIXED_BIN.deserialize(rom.getFileByName('BALANCE/fixed.bin'))
    backgrounds = FileType.BG_LIST_DAT.deserialize(rom.getFileByName('MAP_BG/bg_list.dat'))
    md = FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md')).entries[554]
    wan = CharaWanHandler.deserialize(checked_pack(rom.getFileByName('MONSTER/m_ground.bin'))[md.sprite_index])
    statue = render_wan_frame(wan,wan.animGroupData[0][0][0],(64,64))
    wanted = {g for p in data['plans'] for g in p['entryGroundIds']}
    wanted.update(g for p in data['plans'] for f in p['floors'] for g in f['groundIds'])
    result = {}
    output.mkdir(parents=True, exist_ok=True)
    for identity in sorted(wanted):
        g = data['grounds'][identity]
        level = config.script_data.level_list__by_name[g['level']]
        bg = backgrounds.level[level.mapid]
        def load(kind,name,ext): return kind.deserialize(rom.getFileByName(f'MAP_BG/{name.lower()}.{ext}'))
        for ref in g['backgroundSources']:
            if hashlib.sha256(rom.getFileByName(ref['path'])).hexdigest()!=ref['sha256']:
                raise ValueError('audited ground source changed')
        bma = load(FileType.BMA,bg.bma_name,'bma')
        mapped = resolve_mapping_for_level(level,mapping,mappa,fixed,pack,definitions)
        if mapped:
            dma,dpc,dpci,dpl,_,room=mapped
            drawer=DmaDrawer(dma)
            rules=drawer.rules_from_fixed_room(room) if room else drawer.rules_from_bma(bma.layer0,bma.map_width_chunks)
            chunks=drawer.get_mappings_for_rules(rules,treat_outside_as_wall=True,variation_index=0)
            pic=drawer.draw(chunks,dpci,dpc,dpl,None)[0].convert('RGBA')
        else:
            bpc=load(FileType.BPC,bg.bpc_name,'bpc');bpl=load(FileType.BPL,bg.bpl_name,'bpl')
            bpas=[load(FileType.BPA,n,'bpa') if n else None for n in bg.bpa_names]
            original=bpc.chunks_animated_to_pil
            def materialize(*args,**kwargs):
                images=original(*args,**kwargs)
                for image in images:
                    if image.palette and not isinstance(image.palette.palette,(bytes,bytearray,list)):
                        image.palette.palette=bytes(image.palette.palette)
                return images
            bpc.chunks_animated_to_pil=materialize
            pic=bma.to_pil(bpc,bpl,bpas,include_collision=False,include_unknown_data_block=False,pal_ani=False,single_frame=True)[0].convert('RGBA')
        pic.putalpha(255)
        actors=g['statueLayout']['actors'] if g['statueLayout'] else []
        if not actors or g['statueStatus']!='confirmed': raise ValueError('selected ground lacks confirmed statue')
        x=y=0
        for actor in actors:
            pos=actor['position'];x=pos['xRelative']*8+(4 if pos['xOffset']>=2 else 0);y=pos['yRelative']*8+(4 if pos['yOffset']>=2 else 0)
            pic.alpha_composite(statue,(x-32,y-36))
        stem=g['level'].lower();pic.save(output/(stem+'.png'))
        provenance={'romHash':data['romHash'],'groundId':identity,'backgroundSources':g['backgroundSources'],
                    'tilesetMapping':g['tilesetMapping'],'statueLayout':g['statueLayout'],
                    'scope':'original static ground and statue; product resting positions below statue; no original story VM'}
        package={'assetVersion':1,'tickHz':60,'width':pic.width,'height':pic.height,
                 'pngBytes':(output/(stem+'.png')).stat().st_size,'sourceKind':1,'atlasName':stem+'.png',
                 'pngSha256':hashlib.sha256((output/(stem+'.png')).read_bytes()).hexdigest(),
                 'assetKey':'rom:EoS_NA:expedition:'+identity,'provenance':json.dumps(provenance,sort_keys=True),
                 'clips':[{'id':0,'name':g['level'],'rows':[[{'rect':[0,0,pic.width,pic.height], 'anchor':[0,0], 'shadow':[0,0], 'ticks':1,'markers':[]}]]}]}
        (output/(stem+'.mdpa')).write_bytes(encode_package(package))
        # These are product placements; the original statue coordinates stay intact.
        player=[max(24,min(pic.width-48,x-24)),max(48,min(pic.height-24,y+32))]
        partner=[player[0]+24,player[1]]
        result[identity]={'assetFile':stem+'.mdpa','size':[pic.width,pic.height], 'player':player,'partner':partner,
                          'storage':any(i['storageConfirmed'] for i in g['interactions']), 'source':provenance}
    (output/'grounds.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    (output/'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original static grounds and Kangaskhan statue\n')
    return result


def contexts(output, plans, versions, rom_hash):
    w=Writer();w.u32(0x5245444d);w.u32(2);w.string(rom_hash)
    w.u32(len(versions))
    for dungeon,version in versions.items():
        w.u32(dungeon);w.string(f'dungeon-{dungeon}/exploration.mdpe');w.string(version)
    floors={(f['dungeonId'],f['localFloor']):f['profile'] for p in plans for f in p['floors']}
    w.u32(len(floors))
    def weights(rows):
        w.u32(len(rows))
        for row in rows:w.u32(row['originalId']);w.data.extend(struct.pack('<Q',row['relativeWeight']))
    def items(ctx):
        cats=ctx['categories']['entries'];w.u32(len(cats))
        conditional={c['categoryId']:c['entries'] for c in ctx['conditionalItems']}
        for cat in cats:w.u32(cat['originalId']);w.data.extend(struct.pack('<Q',cat['relativeWeight']));weights(conditional.get(cat['originalId'],[]))
    for (dungeon,local),p in sorted(floors.items()):
        w.u32(dungeon);w.u32(local)
        for name in ('kecleon_shop_chance','monster_house_chance','trap_density'):w.u32(p['rawLayout'][name])
        weights(p['monsters']['monsterHouse']['entries'])
        trap=sorted((int(k),v) for k,v in p['traps'].items());relative=cdf_weights([v for _,v in trap])
        weights([{'originalId':k,'relativeWeight':n} for (k,_),n in zip(trap,relative)])
        for context in ('shop_items','monster_house_items'):items(next(c for c in p['itemContexts'] if c['context']==context))
    output.mkdir(parents=True,exist_ok=True);(output/'contexts.mder').write_bytes(w.data)
    (output/'provenance.json').write_text(json.dumps({'romSha256':rom_hash,'maps':[{'dungeonId':k,'assetVersion':v} for k,v in versions.items()],
        'floors':list(floors.values()),'policy':'original contextual weights; automatic expedition choice excludes branch/test/unresolved routes'},ensure_ascii=False,separators=(',',':'))+'\n')


def prepare(root, output):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.util import get_ppmdu_config_for_rom
    data=catalogue(root);branches=branch_plans(root,data)
    source=root/'assets/original/source/user-supplied.nds';before=hashlib.sha256(source.read_bytes()).hexdigest()
    if before!=data['romHash']:raise ValueError('research/ROM identity mismatch')
    rom=NintendoDSRom.fromFile(str(source));config=get_ppmdu_config_for_rom(rom)
    pack=FileType.DUNGEON_BIN.deserialize(rom.getFileByName('DUNGEON/dungeon.bin'),config)
    output.mkdir(parents=True,exist_ok=True)
    stops=ground_assets(root,output/'gameplay/expeditions',data,rom,config,pack)
    all_plans=data['plans']+branches
    fixed_layouts(all_plans,rom)
    tiles=sorted({f['profile']['tilesetId'] for p in all_plans for f in p['floors']})
    shared=output/'tiles';shared.mkdir(exist_ok=True)
    tile_data={t:(export_tileset if t<170 else export_fixed_background)(pack,t,shared,before) for t in tiles}
    versions={}
    for p in all_plans:
        dungeon=p['dungeonIds'][0];dest=output/f'exploration/dungeon-{dungeon}';dest.mkdir(parents=True,exist_ok=True)
        profiles=[f['profile'] for f in p['floors']]
        body=bytearray(write_profiles(profiles));w=Writer()
        selected=sorted({f['tilesetId'] for f in profiles} | ({f['profile']['tilesetId'] for b in branches for f in b['floors']} if p['id']=='ARC-M07' else set()));w.u32(len(selected))
        for identity in selected:
            t=tile_data[identity];w.u32(identity);w.u32(t['count']);w.string(t['package'])
            w.data.extend(struct.pack('<2304I',*t['mappings']))
            for name in (t['package'],f'tileset-{identity}.png'):shutil.copyfile(shared/name,dest/name)
        body.extend(w.data);body.extend(write_plan(p,stops))
        hashes={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(dest.iterdir()) if f.suffix in ('.png','.mdpa')}
        version=hashlib.sha256(bytes(body)+canonical(hashes)).hexdigest();header=Writer()
        header.u32(0x4550444d);header.u32(2);header.string(version);header.string(before)
        (dest/'exploration.mdpe').write_bytes(header.data+body)
        (dest/'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT. Offline parsers: SkyTemple-files (GPL-3.0-or-later).\n')
        if p in data['plans']:versions[dungeon]=version
        print('prepared',p['id'],len(profiles),flush=True)
    contexts(output/'gameplay/events',all_plans,versions,before)
    report={'schemaVersion':1,'romHash':before,'plans':[{k:v for k,v in p.items() if k!='floors'}|{'floors':len(p['floors'])} for p in data['plans']],
            'branches':data['branches'],'excluded':data['excluded'],'tilesets':tiles,
            'fixedPolicy':'original connected fixed terrain within 22x22 cells, leader/actor coordinates and static DBG; unused/outside corridor clipped, exit marker is product policy; existing 2-hit combat, no original stat calculation'}
    (output/'gameplay/expeditions/catalogue.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    if hashlib.sha256(source.read_bytes()).hexdigest()!=before:raise ValueError('ROM changed')
    return {'plans':len(versions),'branches':len(branches),'tilesets':len(tiles),'grounds':len(stops),'romUnchanged':True}


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();print(json.dumps(prepare(a.root,a.out),ensure_ascii=False))
