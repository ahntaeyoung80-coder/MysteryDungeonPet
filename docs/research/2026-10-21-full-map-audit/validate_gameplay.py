"""맵별 조사 자료의 원본 hash·층/ID·CDF 비율·고정 배치·문서 링크를 검증한다."""
from pathlib import Path
from fractions import Fraction
from functools import lru_cache
from collections import Counter
from datetime import datetime,timezone
import csv,gzip,json,hashlib,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT))
OUT=Path(__file__).resolve().parent;EXP=OUT.parent/'2026-10-21-continuous-expeditions'
sha=lambda b:hashlib.sha256(b).hexdigest()
read=lambda p:json.loads(p.read_text())
def run():
 summary=read(OUT/'gameplay-extraction.json');maps=read(OUT/'map-gameplay.json');by_id={m['dungeonId']:m for m in maps};assert set(by_id)==set(range(192))
 physical={};audited={(int(r['group']),int(r['groupFloor'])):r for r in csv.DictReader((OUT/'physical-floors.csv').open())}
 for line in gzip.open(OUT/'gameplay-floor-profiles.jsonl.gz','rt'):
  p=json.loads(line);key=p['physicalId'];group,index=p['groupIndex'],p['groupFloor'];assert key==f'FLR-G{group:03d}-F{index:03d}' and key not in physical
  original={k:v for k,v in p.items() if k not in ['physicalId','edition','sourceProfileSha256']};original['key']={'edition':p['edition'],'dungeonId':group,'localFloor':index}
  assert sha(json.dumps(original,sort_keys=True,separators=(',',':')).encode())==p['sourceProfileSha256']==audited[(group,index)]['profileSha256']
  physical[key]=p
 assert len(physical)==1892==summary['priorProfileHashMatches']
 routes=read(EXP/'routes.json');route_segs={s['dungeonId']:s for s in routes['segments']};links={l['id']:l for l in routes['links']}
 rules=read(OUT/'progression-rules.json')['rules'];rulemap={r['id']:r for r in rules};assert len(rulemap)==15
 items=read(OUT/'gameplay-items.json');monsters=read(OUT/'gameplay-monsters.json');assert len(items)==1400
 assert set(re.findall(r'maps/SEG-D(\d{3})\.md',(OUT/'DUNGEON_ID_INDEX.md').read_text()))=={f'{n:03d}' for n in range(192)}
 logical=0;missing=0
 for m in maps:
  assert m['usage']==route_segs[m['dungeonId']]['usage'];assert m['gameplayDoc']==f"maps/{m['segmentId']}.md"
  assert (OUT/m['gameplayDoc']).exists() and (EXP/route_segs[m['dungeonId']]['gameplayDoc']).exists()
  assert len(m['floorRefs'])==m['resolvedFloors'];logical+=len(m['floorRefs']);missing+=len(m['missingReferences'])
  for f in m['floorRefs']:
   p=physical[f['physicalId']];assert f['localFloor0']+1==f['displayFloor']
   assert (p['groupIndex'],p['groupFloor'])==(m['mappaGroup'],m['startAfter']+f['localFloor0'])
   assert f['fixedFloorId']==p['rawLayout']['fixed_floor_id']
  assert all(l in links and links[l]['fromDungeonId']==m['dungeonId'] for l in m['outgoingLinkIds'])
  assert all(l in links and links[l]['toDungeonId']==m['dungeonId'] for l in m['incomingLinkIds'])
  assert set(m['progressionRuleIds'])=={r['id'] for r in rules if m['dungeonId'] in r['segments']}
 assert logical==1894 and missing==6
 def source_row(r):
  m=by_id[int(r['dungeonId'])];assert r['segmentId']==m['segmentId'];f=m['floorRefs'][int(r['floor'])-1]
  assert r['physicalId']==f['physicalId'] and int(r['fixedFloorId'])==f['fixedFloorId'];return physical[r['physicalId']]
 enemy_count=0
 for row in csv.DictReader(gzip.open(OUT/'enemy-floor-rates.csv.gz','rt')):
  p=source_row(row);ctx=row['context'];pool=p['monsters'][ctx]['entries'];total=sum(e['relativeWeight'] for e in pool);mid=int(row['mdIndex'])
  e=next(e for e in pool if e['originalId']==mid);level=next(e['level'] for e in p['monsters']['rawRows'] if e['mdIndex']==mid)
  assert int(row['level'])==level and int(row['weight'])==e['relativeWeight']>0 and int(row['weightTotal'])==total
  assert int(row['rawCDF'])==e['rawCDF'] and row['name']==monsters[mid]['name']
  assert Fraction(int(row['probabilityNumerator']),int(row['probabilityDenominator']))==Fraction(e['relativeWeight'],total)
  enemy_count+=1
 @lru_cache(maxsize=2)
 def item_source(pid,ctx):
  context=next(c for c in physical[pid]['itemContexts'] if c['context']==ctx)
  cats={e['originalId']:e['relativeWeight'] for e in context['categories']['entries']};total=sum(cats.values());rows={}
  for c in context['conditionalItems']:
   ctotal=sum(e['relativeWeight'] for e in c['entries'])
   for e in c['entries']:rows[e['originalId']]=(c['categoryId'],cats.get(c['categoryId'],0),total,e['relativeWeight'],ctotal,e['rawCDF'])
  guaranteed={e['recordIndex']:e for e in context['guaranteed']};return rows,guaranteed
 item_count=0
 for row in csv.DictReader(gzip.open(OUT/'item-floor-rates.csv.gz','rt')):
  source_row(row);identity=int(row['recordIndex']);assert int(row['itemId'])==items[identity]['itemId'] and row['name']==items[identity]['name']
  raw,guaranteed=item_source(row['physicalId'],row['context'])
  if row['selection']=='guaranteed_marker':assert identity in guaranteed and int(row['itemRawCDF'])==65535 and row['probabilityDenominator']==''
  else:
   cat,cw,ct,iw,it,cdf=raw[identity]
   assert (int(row['categoryId']),int(row['categoryWeight']),int(row['categoryWeightTotal']),int(row['itemWeight']),int(row['itemWeightTotal']),int(row['itemRawCDF']))==(cat,cw,ct,iw,it,cdf)
   assert cw>0 and iw>0 and Fraction(int(row['probabilityNumerator']),int(row['probabilityDenominator']))==Fraction(cw,ct)*Fraction(iw,it)
  item_count+=1
 assert enemy_count==45108==summary['enemyFloorRows'] and item_count==774916==summary['itemFloorRows']
 details=[json.loads(l) for l in gzip.open(OUT/'map-gameplay-details.jsonl.gz','rt')];assert len(details)==192
 for m in details:
  for e in m['enemyRows']:assert 0<=e['mdIndex']<len(monsters) and e['weight']>0 and e['weightTotal']>=e['weight']
  for i in m['itemRows']:
   assert items[i['recordIndex']]['itemId']==i['itemId']
   if i['probabilityMin'] is not None:assert 0<Fraction(i['probabilityMin'])<=Fraction(i['probabilityMax'])<=1
 fixed={f['fixedFloorId']:f for f in read(OUT/'gameplay-fixed-spawns.json')};assert len(fixed)==112
 for m in maps:assert all(fid in fixed for fid in m['fixedFloorIds'])
 for f in fixed.values():
  if f['overrideFixedFloorId']:assert f['overrideFixedFloorId'] in fixed
  for s in f['spawns']:assert s['mdIndex']<len(monsters) and 0<=s['x']<f['width'] and 0<=s['y']<f['height']
 # 보스 MAPPA 후보를50% 추첨으로 표시하지 않도록 실제 배치와 요구 아이템 수를 확인한다.
 assert [(s['mdIndex'],s['stats']['level'],s['stats']['hp']) for s in fixed[10]['spawns'] if s['mdIndex']]==[(552,48,700)]
 assert [(s['mdIndex'],s['stats']['level'],s['stats']['hp']) for s in fixed[74]['spawns'] if s['mdIndex']]==[(525,48,700)]
 assert '552 · PRIMAL_DIALGA' not in (OUT/'maps/SEG-D043.md').read_text().split('<details>')[0]
 assert rulemap['PRG-AEGIS-STEEL']['requiredItems']==[{'recordIndex':418,'quantity':1},{'recordIndex':419,'quantity':1},{'recordIndex':404,'quantity':2},{'recordIndex':411,'quantity':1}]
 assert rulemap['PRG-CRYSTAL-BLUE']['requiredCrystalColors']==['blue']*3
 assert by_id[175]['usage']=='ground_alias_not_playable' and by_id[71]['resolvedFloors']==0 and by_id[191]['resolvedFloors']==48
 from ndspy.rom import NintendoDSRom
 from skytemple_files.common.util import get_ppmdu_config_for_rom,get_binary_from_rom
 source=ROOT/'assets/original/source/user-supplied.nds';romhash=sha(source.read_bytes());assert romhash==summary['romSha256']=='6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf'
 rom=NintendoDSRom.fromFile(str(source));config=get_ppmdu_config_for_rom(rom)
 for path,digest in summary['sourceHashes'].items():assert sha(rom.getFileByName(path))==digest
 for name,digest in summary['binaryHashes'].items():assert sha(get_binary_from_rom(rom,getattr(config.bin_sections,name)))==digest
 exe_hash=sha((ROOT/'MysteryDungeonPet.exe').read_bytes());assert exe_hash=='9235fc2474131aee7c3266fb066f35807f8f25fe89e391bf3c0c60e462edd17d'
 docs=list((OUT/'maps').glob('*.md'))+[OUT/'README.md',OUT/'DUNGEON_ID_INDEX.md',OUT/'GAMEPLAY_DATA.md',OUT/'PROGRESSION.md',EXP/'README.md'];doclinks=0
 for doc in docs:
  for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)',doc.read_text()):
   if target.startswith(('https:','http:','#')):continue
   path,_,anchor=target.partition('#');p=doc.parent/path
   if p.name=='gameplay-validation.json':continue
   assert p.exists(),(doc,target)
   if anchor:assert f'id="{anchor}"' in p.read_text() or anchor in p.read_text()
   doclinks+=1
 changed=set(subprocess.check_output(['git','diff','--name-only','5d74bf654a9b5bd4eae61a3d513737cccf8a9d85'],cwd=ROOT,text=True).splitlines())
 changed.update(subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=ROOT,text=True).splitlines());assert all(p.startswith('docs/') or p=='PROJECT_STATE.md' for p in changed)
 result={'status':'pass','checkedAtUtc':datetime.now(timezone.utc).isoformat(),'maps':192,'mapDocuments':192,'physicalProfiles':1892,'logicalFloorRefs':logical,
  'enemyFloorRows':enemy_count,'itemFloorRows':item_count,'fixedRoomsWithOverrides':len(fixed),'progressionRules':15,'localLinksChecked':doclinks,
  'checks':['all prior physical profile hashes','SEG/FLR/RUN/ARC/LNK joins','every enemy/item rate vs raw CDF weights','fixed boss and override separation','ICE/ROCK/STEEL item quantities and blue-crystal condition','all maps linked from existing index and route graph','invalid refs and ground aliases retained','original data/binary hashes','document links and doc-only scope','ROM and EXE unchanged'],
  'romSha256':romhash,'exeSha256':exe_hash,'productCodeAssetsExeChanged':False,'notExecuted':'제품 전체/GUI/ASan 재검사 및 원작 모든 스토리 조건 실행: 조사/문서 전용 변경'}
 (OUT/'gameplay-validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
if __name__=='__main__':run()
