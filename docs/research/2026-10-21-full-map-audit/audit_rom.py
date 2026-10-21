"""보관 ROM 전체 파일·MAPPA 물리 층·일반/도장 ID를 읽기 전용으로 대조한다.
실행: PYTHONPATH=. local/p1-venv/bin/python docs/research/2026-10-21-full-map-audit/audit_rom.py
"""
from pathlib import Path
from types import SimpleNamespace
from collections import Counter
from datetime import datetime, timezone
import csv,gzip,hashlib,json,sys,time
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT))
from tools.mdpet.catalogue import build_floor
from ndspy.rom import NintendoDSRom
from skytemple_files.common.types.file_types import FileType
from skytemple_files.common.util import get_ppmdu_config_for_rom,get_binary_from_rom
from skytemple_files.common.impl_cfg import ImplementationType,change_implementation_type
from skytemple_files.hardcoded.dungeons import HardcodedDungeons
OUT=Path(__file__).resolve().parent
hashbytes=lambda b:hashlib.sha256(b).hexdigest()
def write(name,data): (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
def table(name,rows):
    with (OUT/name).open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
def main():
    started=time.monotonic();change_implementation_type(ImplementationType.PYTHON)
    source=ROOT/'assets/original/source/user-supplied.nds';before=hashbytes(source.read_bytes())
    rom=NintendoDSRom.fromFile(str(source));config=get_ppmdu_config_for_rom(rom)
    symbols=json.loads((OUT/'enum-identities.json').read_text())
    paths={}
    def walk(folder,prefix=''):
        for index,name in enumerate(folder.files):paths[folder.firstID+index]=prefix+name
        for name,child in folder.folders:walk(child,prefix+name+'/')
    walk(rom.filenames)
    manifest=[{'fileId':i,'path':paths.get(i,'<no FNT name>'),'bytes':len(data),'sha256':hashbytes(data)} for i,data in enumerate(rom.files)]
    table('rom-files.csv',manifest)
    arm9=get_binary_from_rom(rom,config.bin_sections.arm9)
    definitions=HardcodedDungeons.get_dungeon_list(arm9,config)
    restrictions=HardcodedDungeons.get_dungeon_restrictions(arm9,config)
    mappa=FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'))
    try:
        ground=FileType.MAPPA_G_BIN.deserialize(rom.getFileByName('BALANCE/mappa_gs.bin'))
        ground_stats={'groups':len(ground.floor_lists),'floors':sum(map(len,ground.floor_lists)),'layoutMismatches':[[g,f] for g,entries in enumerate(ground.floor_lists) for f,entry in enumerate(entries) if (int(entry.layout.tileset_id),int(entry.layout.fixed_floor_id))!=(int(mappa.floor_lists[g][f].layout.tileset_id),int(mappa.floor_lists[g][f].layout.fixed_floor_id))]}
    except Exception as e:ground_stats={'error':str(e)}
    md=FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md'))
    raw_items=FileType.ITEM_P.deserialize(rom.getFileByName('BALANCE/item_p.bin')).item_list
    items=[{'itemId':int(i.item_id),'categoryId':int(i.category)} for i in raw_items]
    members={int(k):v.items for k,v in config.dungeon_data.item_categories.items()}
    physical={};floors=[];errors=[]
    for group,entries in enumerate(mappa.floor_lists):
        definition=SimpleNamespace(mappa_index=group,start_after=0,number_floors=len(entries))
        for local,entry in enumerate(entries):
            layout=entry.layout
            row={'group':group,'groupFloor':local,'tileset':int(layout.tileset_id),'fixedFloor':int(layout.fixed_floor_id),'normalCandidates':0,'mhCandidates':0,'itemContexts':0,'profileSha256':'','error':''}
            try:
                profile=build_floor(config.game_edition,group,local,definition,mappa.floor_lists,md.entries,items,members)
                row.update(normalCandidates=sum(i['relativeWeight']>0 for i in profile['monsters']['normal']['entries']),mhCandidates=sum(i['relativeWeight']>0 for i in profile['monsters']['monsterHouse']['entries']),itemContexts=len(profile['itemContexts']),profileSha256=hashbytes(json.dumps(profile,sort_keys=True,separators=(',',':')).encode()))
                physical[(group,local)]=profile
            except Exception as e:
                row['error']=str(e);errors.append({'group':group,'groupFloor':local,'error':str(e)})
            floors.append(row)
    table('physical-floors.csv',floors)
    by_key={(r['group'],r['groupFloor']):r for r in floors}
    packaged={int(p.parent.name.split('-')[1]) for p in (ROOT/'assets/original/exploration').glob('dungeon-*/exploration.mdpe')}
    definitions_out=[];covered=set();range_errors=[]
    for dungeon,d in enumerate(definitions):
        group,start,length=int(d.mappa_index),int(d.start_after),int(d.number_floors)
        refs=[(group,start+i) for i in range(length)];valid=[r for r in refs if r in by_key];covered.update(valid)
        missing=[r for r in refs if r not in by_key]
        for g,f in missing:range_errors.append({'dungeon':dungeon,'group':g,'groupFloor':f})
        row={'dungeonId':dungeon,'symbol':symbols['dungeonIds'][str(dungeon)],'configName':config.dungeon_data.dungeons[dungeon].name,'group':group,'startAfter':start,'declaredFloors':length,'resolvedFloors':len(valid),'fixedFloors':sum(by_key[r]['fixedFloor']>0 for r in valid),'tilesets':sorted({by_key[r]['tileset'] for r in valid}),'normalMdIds':sorted({i['originalId'] for r in valid if r in physical for i in physical[r]['monsters']['normal']['entries'] if i['relativeWeight']}),'hasRuntimePackage':dungeon in packaged,'missingReferences':missing}
        definitions_out.append(row)
    dojo=[]
    for dungeon in range(180,192):
        start=(dungeon-180)*5 if dungeon<190 else 50 if dungeon==190 else 51
        count=5 if dungeon<190 else 1 if dungeon==190 else 48
        refs=[(53,start+i) for i in range(count)];assert all(r in by_key for r in refs);covered.update(refs)
        dojo.append({'dungeonId':dungeon,'symbol':symbols['dungeonIds'][str(dungeon)],'configName':config.dungeon_data.dungeons[dungeon].name,'group':53,'startAfter':start,'declaredFloors':count,'resolvedFloors':count,'fixedFloors':sum(by_key[r]['fixedFloor']>0 for r in refs),'tilesets':sorted({by_key[r]['tileset'] for r in refs}),'normalMdIds':sorted({i['originalId'] for r in refs if r in physical for i in physical[r]['monsters']['normal']['entries'] if i['relativeWeight']}),'hasRuntimePackage':False,'missingReferences':[]})
    write('dungeon-definitions.json',definitions_out+dojo)
    prior=json.loads((ROOT/'docs/research/2026-10-20-variety-data/audit-summary.json').read_text())
    hash_matches={path:hashbytes(rom.getFileByName(path))==data['sha256'] for path,data in prior['sourceHashes'].items()}
    comparisons=0;mismatches=[]
    with gzip.open(ROOT/'docs/research/2026-10-20-variety-data/floor-profiles.jsonl.gz','rt') as f:
        for line in f:
            old=json.loads(line);fresh=physical.get((old['groupIndex'],old['groupFloor']))
            if fresh is None:mismatches.append({'dungeon':old['dungeonId'],'floor':old['localFloor'],'field':'missing physical'});continue
            for old_name,new_name in [('normal','normal'),('monsterHouse','monsterHouse')]:
                a=sorted((int(i['mdIndex']),int(i['weight'])) for i in old[old_name] if i['weight'])
                b=sorted((int(i['originalId']),int(i['relativeWeight'])) for i in fresh['monsters'][new_name]['entries'] if i['relativeWeight'])
                if a!=b:mismatches.append({'dungeon':old['dungeonId'],'floor':old['localFloor'],'field':old_name})
            if old['layout']!=fresh['rawLayout']:mismatches.append({'dungeon':old['dungeonId'],'floor':old['localFloor'],'field':'layout'})
            comparisons+=1
    # Named physical route families, excluding test/system/unnamed reserves; dojo is a separate family.
    plan=json.loads((OUT/'web-plan.json').read_text())['rows'];groups=[]
    for spec in plan:
        g=spec['group'];canonical=[d for d in definitions_out if d['group']==g and d['dungeonId'] not in (175,176,177,178,179)] if g!=53 else dojo
        refs={(g,d['startAfter']+i) for d in canonical for i in range(d['resolvedFloors'])};entries=[by_key[r] for r in sorted(refs)]
        groups.append({**spec,'dungeonIds':[d['dungeonId'] for d in canonical],'declaredFloors':sum(d['declaredFloors'] for d in canonical),'resolvedFloors':sum(d['resolvedFloors'] for d in canonical),'physicalFloors':len(mappa.floor_lists[g]),'unreferencedGroupFloors':[f for f in range(len(mappa.floor_lists[g])) if (g,f) not in refs],'fixedFloors':sum(r['fixedFloor']>0 for r in entries),'proceduralFloors':sum(r['fixedFloor']==0 for r in entries),'tilesets':sorted({r['tileset'] for r in entries}),'packagedIds':[d['dungeonId'] for d in canonical if d['hasRuntimePackage']]})
    write('route-groups.json',groups)
    identity=[]
    for dungeon in range(256):
        kind='general definition' if dungeon<180 else 'dojo mapping' if dungeon<192 else 'dojo alias/reserved id' if dungeon<212 else 'system/name/metainfo id without general floor definition'
        identity.append({'id':dungeon,'symbol':symbols['dungeonIds'][str(dungeon)],'kind':kind})
    table('all-256-identities.csv',identity)
    after=hashbytes(source.read_bytes());assert before==after
    summary={'generatedUtc':datetime.now(timezone.utc).isoformat(),'seconds':time.monotonic()-started,'romSha256':before,'romUnchanged':True,'gameCode':rom.idCode.decode(),'edition':config.game_edition,'romFiles':len(manifest),'namedRomFiles':len(paths),'generalDefinitions':len(definitions),'restrictionIdSlots':len(restrictions),'dojoDefinitions':len(dojo),'physicalMappaGroups':len(mappa.floor_lists),'physicalMappaFloors':len(floors),'floorParseErrors':errors,'generalLogicalFloorsDeclared':sum(d['declaredFloors'] for d in definitions_out),'generalLogicalFloorsResolved':sum(d['resolvedFloors'] for d in definitions_out),'rangeErrors':range_errors,'dojoPhysicalFloors':len(mappa.floor_lists[53]),'allDefinitionCoveredPhysicalFloors':len(covered),'unreferencedPhysicalFloors':[list(r) for r in sorted(set(by_key)-covered)],'mappaGround':ground_stats,'priorSourceHashMatch':hash_matches,'priorLogicalFloorsCompared':comparisons,'priorMismatch':mismatches,'namedRouteGroups':len(groups),'routeCategoryCounts':dict(Counter(g['category'] for g in groups)),'runtimePackageIds':sorted(packaged),'defaultPoolIds':[15,18],'tileIds':sorted({r['tileset'] for r in floors}),'sourceFiles':{p:hashbytes(rom.getFileByName(p)) for p in ['BALANCE/mappa_s.bin','BALANCE/mappa_gs.bin','BALANCE/monster.md','BALANCE/item_p.bin','DUNGEON/dungeon.bin','MAP_BG/bg_list.dat']}}
    write('summary.json',summary);print(json.dumps({k:v for k,v in summary.items() if k not in ('tileIds','sourceFiles','unreferencedPhysicalFloors','priorSourceHashMatch')},ensure_ascii=False))
if __name__=='__main__':main()
