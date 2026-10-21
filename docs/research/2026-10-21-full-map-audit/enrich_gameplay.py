"""전체 원작 맵의 적·아이템·고정 배치·제약과 기존 연속 원정 ID를 결합한다.
조사 자료만 생성한다. 제품 런타임/선택 자산을 수정하지 않는다.
"""
from pathlib import Path
from types import SimpleNamespace
from collections import defaultdict
from fractions import Fraction
import csv,gzip,hashlib,json,sys,time
from datetime import datetime,timezone
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT))
OUT=Path(__file__).resolve().parent
from tools.mdpet.catalogue import build_floor,scalar
from ndspy.rom import NintendoDSRom
from skytemple_files.common.types.file_types import FileType
from skytemple_files.common.util import get_ppmdu_config_for_rom,get_binary_from_rom
from skytemple_files.common.impl_cfg import ImplementationType,change_implementation_type
from skytemple_files.hardcoded.dungeons import HardcodedDungeons
from skytemple_files.hardcoded.fixed_floor import HardcodedFixedFloorTables as Fixed
from skytemple_files.dungeon_data.fixed_bin.model import EntityRule
sha=lambda b:hashlib.sha256(b).hexdigest()
canonical=lambda x:json.dumps(x,sort_keys=True,separators=(',',':')).encode()
def write(name,value):(OUT/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
def zipped(name,rows):
 with gzip.GzipFile(filename=str(OUT/name),mode='wb',mtime=0) as f:
  for row in rows:f.write(json.dumps(row,ensure_ascii=False,separators=(',',':')).encode()+b'\n')
def ranges(values):
 result=[]
 for x in sorted(set(values)):
  if result and result[-1][-1]+1==x:result[-1].append(x)
  else:result.append([x])
 return ','.join(str(r[0]) if len(r)==1 else f'{r[0]}-{r[-1]}' for r in result)
def run():
 started=time.monotonic();change_implementation_type(ImplementationType.PYTHON)
 source=ROOT/'assets/original/source/user-supplied.nds';before=sha(source.read_bytes());rom=NintendoDSRom.fromFile(str(source));c=get_ppmdu_config_for_rom(rom)
 identities=json.loads((OUT/'gameplay-identities.json').read_text());names=identities['monsterIds']
 md=FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md'));ip=FileType.ITEM_P.deserialize(rom.getFileByName('BALANCE/item_p.bin')).item_list
 old=list(csv.DictReader((OUT.parent/'2026-10-20-variety-data/items.csv').open()));assert len(old)==len(ip)
 items=[]
 for index,it in enumerate(ip):
  for field,actual in [('itemId',it.item_id),('categoryId',it.category),('sprite',it.sprite),('palette',it.palette)]:assert int(old[index][field])==int(actual)
  items.append({'recordIndex':index,'name':old[index]['staticName'],'nameSource':'SkyTemple static config; patched Korean ROM text undecoded',
   'itemId':int(it.item_id),'categoryId':int(it.category),'categoryName':old[index]['categoryName'],
   'sprite':int(it.sprite),'palette':int(it.palette),'isValid':bool(it.is_valid),'buyPrice':int(it.buy_price),'sellPrice':int(it.sell_price),'moveId':int(it.move_id)})
 write('gameplay-items.json',items)
 monsters=[{'mdIndex':i,'name':names.get(str(i),f'MD_{i}_UNRESOLVED'),'nationalDex':int(x.national_pokedex_number),
  'spriteIndex':int(x.sprite_index),'baseFormIndex':int(x.base_form_index),'gender':int(x.gender),
  'typePrimary':int(x.type_primary),'typeSecondary':int(x.type_secondary),'movementType':int(x.movement_type),
  'abilityPrimary':int(x.ability_primary),'abilitySecondary':int(x.ability_secondary),'itemRequiredForSpawning':bool(x.item_required_for_spawning)} for i,x in enumerate(md.entries)]
 write('gameplay-monsters.json',monsters)
 members={int(k):v.items for k,v in c.dungeon_data.item_categories.items()}
 mappa=FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'))
 audited={(int(r['group']),int(r['groupFloor'])):r for r in csv.DictReader((OUT/'physical-floors.csv').open())}
 physical={};source_matches=0
 for group,floors in enumerate(mappa.floor_lists):
  definition=SimpleNamespace(mappa_index=group,start_after=0,number_floors=len(floors))
  for index in range(len(floors)):
   p=build_floor(c.game_edition,group,index,definition,mappa.floor_lists,md.entries,items,members)
   digest=sha(canonical(p));assert digest==audited[(group,index)]['profileSha256'];source_matches+=1
   p.pop('key');p.update(physicalId=f'FLR-G{group:03d}-F{index:03d}',edition=c.game_edition,sourceProfileSha256=digest)
   physical[(group,index)]=p
 zipped('gameplay-floor-profiles.jsonl.gz',physical.values())
 arm9=get_binary_from_rom(rom,c.bin_sections.arm9);restrictions=HardcodedDungeons.get_dungeon_restrictions(arm9,c)
 ov29=get_binary_from_rom(rom,c.bin_sections.overlay29);ov10=get_binary_from_rom(rom,c.bin_sections.overlay10)
 entities=Fixed.get_entity_spawn_table(ov29,c);fixed_mon=Fixed.get_monster_spawn_list(ov29,c);fixed_it=Fixed.get_item_spawn_list(ov29,c)
 stats=Fixed.get_monster_spawn_stats_table(ov10,c);overrides=Fixed.get_fixed_floor_overrides(ov29,c)
 fbin=FileType.FIXED_BIN.deserialize(rom.getFileByName('BALANCE/fixed.bin'));fixed_rows={}
 used={p['rawLayout']['fixed_floor_id'] for p in physical.values()}-{0};used|={int(overrides[f]) for f in used if overrides[f]}
 for fid in sorted(used):
  floor=fbin.fixed_floors[fid];spawns=[]
  for pos,action in enumerate(floor.actions):
   if not isinstance(action,EntityRule):continue
   e=entities[action.entity_rule_id];mon=fixed_mon[e.monster_id];item=fixed_it[e.item_id]
   spawns.append({'x':pos%floor.width,'y':pos//floor.width,'entityRuleId':action.entity_rule_id,
    'monsterSpawnTableIndex':e.monster_id,'mdIndex':int(mon.md_idx),'spawnType':int(mon.enemy_settings.value),'spawnTypeName':mon.enemy_settings.name,
    'statsEntry':int(mon.stats_entry),'stats':{k:scalar(v) for k,v in vars(stats[mon.stats_entry]).items()},
    'statsAppliedByType':mon.enemy_settings.value in (6,10),'itemSpawnTableIndex':e.item_id,'itemId':int(item.item_id),'quantity':int(item.quantity)})
  fixed_rows[fid]={'fixedFloorId':fid,'width':int(floor.width),'height':int(floor.height),'overrideFixedFloorId':int(overrides[fid]),'spawns':spawns}
 write('gameplay-fixed-spawns.json',list(fixed_rows.values()))
 routes=json.loads((OUT.parent/'2026-10-21-continuous-expeditions/routes.json').read_text());defs=json.loads((OUT/'dungeon-definitions.json').read_text());segs={s['dungeonId']:s for s in routes['segments']}
 enemies=[];item_rates=[];maps=[]
 for d in defs:
  did=d['dungeonId'];seg=segs[did];logical=[];mon_groups=defaultdict(list);item_groups=defaultdict(list);layouts=[]
  for local in range(d['declaredFloors']):
   p=physical.get((d['group'],d['startAfter']+local))
   if p is None:continue
   number=local+1;fixed_id=p['rawLayout']['fixed_floor_id'];logical.append({'localFloor0':local,'displayFloor':number,'physicalId':p['physicalId'],'fixedFloorId':fixed_id})
   layouts.append({'floor':number,**p['rawLayout']})
   level={row['mdIndex']:row['level'] for row in p['monsters']['rawRows']}
   for context in ['normal','monsterHouse']:
    pool=p['monsters'][context]['entries'];total=sum(row['relativeWeight'] for row in pool)
    for row in pool:
     if row['relativeWeight']<=0:continue
     identity=row['originalId'];weight=row['relativeWeight'];prob=Fraction(weight,total)
     rowout={'segmentId':seg['id'],'dungeonId':did,'floor':number,'physicalId':p['physicalId'],'fixedFloorId':fixed_id,'context':context,
      'mdIndex':identity,'name':monsters[identity]['name'],'level':level[identity],'rawCDF':row['rawCDF'],'weight':weight,'weightTotal':total,
      'probabilityNumerator':prob.numerator,'probabilityDenominator':prob.denominator,'additionalSpawnCondition':monsters[identity]['itemRequiredForSpawning']}
     enemies.append(rowout)
     mon_groups[(context,identity,level[identity],weight,total,bool(fixed_id))].append(number)
   for context in p['itemContexts']:
    cat_rows=context['categories']['entries'];ctotal=sum(row['relativeWeight'] for row in cat_rows);cat_weights={row['originalId']:row['relativeWeight'] for row in cat_rows}
    for conditional in context['conditionalItems']:
     cat=conditional['categoryId'];rows=conditional['entries'];itotal=sum(row['relativeWeight'] for row in rows)
     for row in rows:
      if row['relativeWeight']<=0 or not cat_weights.get(cat):continue
      identity=row['originalId'];cw=cat_weights[cat];iw=row['relativeWeight'];prob=Fraction(cw,ctotal)*Fraction(iw,itotal)
      item_rates.append({'segmentId':seg['id'],'dungeonId':did,'floor':number,'physicalId':p['physicalId'],'fixedFloorId':fixed_id,'context':context['context'],
       'recordIndex':identity,'itemId':items[identity]['itemId'],'name':items[identity]['name'],'categoryId':cat,
       'categoryWeight':cw,'categoryWeightTotal':ctotal,'itemWeight':iw,'itemWeightTotal':itotal,
       'itemRawCDF':row['rawCDF'],'probabilityNumerator':prob.numerator,'probabilityDenominator':prob.denominator,'selection':'random'})
      item_groups[(context['context'],cat,identity,'random',bool(fixed_id))].append((number,prob))
    for row in context['guaranteed']:
     identity=row['recordIndex'];item_groups[(context['context'],row['categoryId'],identity,'guaranteed_marker',bool(fixed_id))].append((number,None))
     item_rates.append({'segmentId':seg['id'],'dungeonId':did,'floor':number,'physicalId':p['physicalId'],'fixedFloorId':fixed_id,'context':context['context'],
      'recordIndex':identity,'itemId':items[identity]['itemId'],'name':items[identity]['name'],'categoryId':row['categoryId'],'categoryWeight':'','categoryWeightTotal':'','itemWeight':'','itemWeightTotal':'','itemRawCDF':65535,'probabilityNumerator':'','probabilityDenominator':'','selection':'guaranteed_marker'})
   # category65535는 상위 무작위 선택과 다르다. 하위 후보를 누락하지 않고 별도 문맥으로 보존한다.
    for cat in context['categories']['guaranteed']:
     for conditional in context['conditionalItems']:
      if conditional['categoryId']!=cat:continue
      for row in conditional['entries']:
       if row['relativeWeight']>0:item_groups[(context['context'],cat,row['originalId'],'guaranteed_category',bool(fixed_id))].append((number,None))
  incoming=[l for l in routes['links'] if l['toDungeonId']==did];outgoing=[l for l in routes['links'] if l['fromDungeonId']==did]
  maprow={'segmentId':seg['id'],'dungeonId':did,'name':d['configName'],'symbol':d['symbol'],'familyId':seg['familyId'],'usage':seg['usage'],
   'mappaGroup':d['group'],'startAfter':d['startAfter'],'declaredFloors':d['declaredFloors'],'resolvedFloors':len(logical),
   'floorRefs':logical,'restrictionsRaw':{k:scalar(v) for k,v in vars(restrictions[did]).items()},
   'incomingLinkIds':[x['id'] for x in incoming],'outgoingLinkIds':[x['id'] for x in outgoing],
   'runIds':[r['id'] for r in routes['sectionRoutes'] if did in r['dungeonIds']],
   'arcIds':[r['id'] for r in routes['storyArcs'] if did in r['dungeonIds']],
   'connectors':[x['id'] for x in routes['connectors'] if did in [x['fromDungeonId'],x['toDungeonId']]],
   'enemyRows':[{'context':k[0],'mdIndex':k[1],'name':monsters[k[1]]['name'],'level':k[2],'weight':k[3],'weightTotal':k[4],'fixedFloorOnly':k[5],'floors':ranges(v)} for k,v in sorted(mon_groups.items())],
   'itemRows':[{'context':k[0],'categoryId':k[1],'recordIndex':k[2],'itemId':items[k[2]]['itemId'],'name':items[k[2]]['name'],'selection':k[3],'fixedFloorOnly':k[4],
    'floors':ranges(n for n,_ in v),'probabilityMin':str(min(p for _,p in v if p is not None)) if any(p is not None for _,p in v) else None,
    'probabilityMax':str(max(p for _,p in v if p is not None)) if any(p is not None for _,p in v) else None} for k,v in sorted(item_groups.items())],
   'layouts':layouts,'fixedFloorIds':sorted({f['fixedFloorId'] for f in logical if f['fixedFloorId']}),
   'missingReferences':d['missingReferences'],'gameplayDoc':f"maps/SEG-D{did:03d}.md"}
  maps.append(maprow)
 zipped('map-gameplay-details.jsonl.gz',maps)
 index=[]
 for row in maps:
  small={k:v for k,v in row.items() if k not in ('enemyRows','itemRows','layouts')}
  small['enemySummaryRows']=len(row['enemyRows']);small['itemSummaryRows']=len(row['itemRows'])
  small['detailsSource']='map-gameplay-details.jsonl.gz';index.append(small)
 write('map-gameplay.json',index)
 for name,rows in [('enemy-floor-rates.csv.gz',enemies),('item-floor-rates.csv.gz',item_rates)]:
  with gzip.open(OUT/name,'wt',newline='') as f:
   writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
 after=sha(source.read_bytes());assert before==after
 write('gameplay-extraction.json',{'checkedAtUtc':datetime.now(timezone.utc).isoformat(),'seconds':time.monotonic()-started,'romSha256':before,'romUnchanged':True,
  'physicalFloors':len(physical),'priorProfileHashMatches':source_matches,'maps':len(maps),'monsters':len(monsters),'items':len(items),'fixedRoomsWithOverrides':len(fixed_rows),
  'enemyFloorRows':len(enemies),'itemFloorRows':len(item_rates),'missingReferences':[{'segmentId':m['segmentId'],'missing':m['missingReferences']} for m in maps if m['missingReferences']],
  'sourceHashes':{path:sha(rom.getFileByName(path)) for path in ['BALANCE/mappa_s.bin','BALANCE/monster.md','BALANCE/item_p.bin','BALANCE/fixed.bin']},
  'binaryHashes':{'arm9':sha(arm9),'overlay10':sha(ov10),'overlay29':sha(ov29)},'scope':'원작 수치/후보/정적 진행 연결; 실제 생성 확률·모든 스토리 분기 실행 결과 아님'})
 print('maps',len(maps),'physical',len(physical),'enemy rows',len(enemies),'item rows',len(item_rates),'fixed rooms',len(fixed_rows))
if __name__=='__main__':run()
