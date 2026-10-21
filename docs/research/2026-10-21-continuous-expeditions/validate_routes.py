"""분류 외래키·스토리 예외·ROM 출처와 문서 범위를 최종 확인한다."""
from pathlib import Path
import json,csv,hashlib,re,subprocess
from datetime import datetime,timezone
from ndspy.rom import NintendoDSRom
ROOT=Path(__file__).resolve().parents[3];OUT=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
read=lambda n:json.loads((OUT/n).read_text())
def run():
 r=read('routes.json');gs=read('grounds.json');g={x['id']:x for x in gs};segments={s['id']:s for s in r['segments']};families={s['id']:s for s in r['families']};links={s['id']:s for s in r['links']}
 for rows in [gs,r['segments'],r['families'],r['links'],r['sectionRoutes'],r['storyArcs'],r['connectors']]:assert len(rows)==len({x['id'] for x in rows})
 assert len(segments)==192 and {s['dungeonId'] for s in segments.values()}==set(range(192));assert len(families)==100
 for s in segments.values():assert s['familyId'] in families and s['dungeonId'] in families[s['familyId']]['definitionDungeonIds']
 for l in links.values():
  assert l['from'] in segments and l['to'] in segments
  assert all(x in g for x in l['viaGroundIds'])
  assert set(l['statueGroundIds'])<=set(l['viaGroundIds'])
  assert all(g[x]['statueStatus']=='confirmed' for x in l['statueGroundIds'])
  assert not l['guildReturnBeforeNext'] and l['condition']
 for route in r['sectionRoutes']+r['storyArcs']:
  assert len(route['links'])==len(route['dungeonIds'])-1
  for index,lid in enumerate(route['links']):
   l=links[lid];assert (l['fromDungeonId'],l['toDungeonId'])==tuple(route['dungeonIds'][index:index+2])
  assert all(f'SEG-D{n:03d}' in segments for n in route['dungeonIds'])
 for c in r['connectors']:
  assert c['groundId'] in g and g[c['groundId']]['statueStatus']=='confirmed'
  assert c['save'] and c['storage']==any(i['storageConfirmed'] for i in g[c['groundId']]['interactions'])
 assert len(r['connectors'])==57 and len({c['groundId'] for c in r['connectors']})==57
 assert sum(c['storage'] for c in r['connectors'])==33
 for branch in r['branches']:
  assert branch['fromGroundId']==branch['returnGroundId'] and branch['fromGroundId'] in g
  assert branch['optionalSegment'] in segments
 # 사용자 핵심 사례 및 잘못 직결하기 쉬운 스토리 경계를 독립적으로 고정한다.
 assert links['LNK-D041-D042']['statueGroundIds']==['GND-L068-D30P21A']
 assert links['LNK-D137-D138']['statueGroundIds']==['GND-L119-D48P21A']
 sky=next(x for x in r['sectionRoutes'] if x['id']=='RUN-D111')
 assert sky['dungeonIds']==[111,112,113,114,115,121,116,117,118,119,120,122]
 assert 'LNK-D020-D021' not in links and 'LNK-D020-D021~conditional_revisit' in links
 assert not any(l['fromDungeonId']==62 and l['toDungeonId']==63 for l in links.values())
 assert not any(l['fromDungeonId']==70 and l['toDungeonId']==71 for l in links.values())
 assert all(g[x]['statueStatus']!='confirmed' for x in r['unresolved'][0]['grounds'])
 rom_path=ROOT/'assets/original/source/user-supplied.nds';raw=rom_path.read_bytes();digest=sha(raw)
 assert digest=='6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf'
 rom=NintendoDSRom(raw);manifest=list(csv.DictReader((OUT/'script-manifest.csv').open()))
 assert len(manifest)==3760
 for row in manifest:
  b=rom.getFileByName(row['path']);assert len(b)==int(row['bytes']) and sha(b)==row['sha256']
 refs=0
 for ground in gs:
  for row in ground['backgroundSources']+ground['interactions']+([ground['statueLayout']] if ground['statueLayout'] else []):
   assert sha(rom.getFileByName(row['path']))==row['sha256'];refs+=1
  if ground['interactions']:assert all(i['saveConfirmed'] for i in ground['interactions'])
 for row in read('routing-evidence.json'):
  assert sha(rom.getFileByName(row['path']))==row['sha256'];refs+=1
 for l in links.values():
  for ref in l['destinationOpcodeRefs']:assert sha(rom.getFileByName(ref['path']))==ref['sha256']
 exe=ROOT/'MysteryDungeonPet.exe';exe_hash=sha(exe.read_bytes())
 assert exe_hash=='9235fc2474131aee7c3266fb066f35807f8f25fe89e391bf3c0c60e462edd17d'
 changed=set(subprocess.check_output(['git','diff','--name-only','56b4f77d759307badf8b0164f8fdae851587764b'],cwd=ROOT,text=True).splitlines())
 changed.update(subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=ROOT,text=True).splitlines())
 assert all(p.startswith('docs/') or p=='PROJECT_STATE.md' for p in changed),changed
 for p in [OUT/'README.md',OUT/'EVIDENCE.md']:
  for ref in re.findall(r'\[[^\]]*\]\(([^)]+)\)',p.read_text()):
   if ref.startswith(('https:','http:','#')):continue
   dest=p.parent/ref.split('#')[0]
   if dest.name=='validation.json':continue
   assert dest.exists(),(p,ref)
 assert sha(rom_path.read_bytes())==digest
 summary={'status':'pass','checkedAtUtc':datetime.now(timezone.utc).isoformat(),
  'counts':{'segments':len(segments),'families':len(families),'sectionRoutes':len(r['sectionRoutes']),'storyArcs':len(r['storyArcs']),'links':len(links),'grounds':len(g),'statueGrounds':57,'storageGrounds':33,'scriptHashes':len(manifest),'additionalRomRefs':refs},
  'checks':['ID uniqueness and foreign keys','ordered route edge endpoints','shared grounds and loop branches','main/SE Temporal Tower separation','Sky Peak first-visit battle order','conditional desert revisit and guild-return exclusions','Aegis unresolved status preserved','ROM path and SHA256 references','local document links','documentation-only scope','ROM and product EXE unchanged'],
  'romSha256':digest,'exeSha256':exe_hash,'productCodeAssetsExeChanged':False,
  'notExecuted':['product tests/GUI/EXE/ASan rebuild: research and documentation only','original-game execution of every story flag combination']}
 (OUT/'validation.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
 files={p.name:{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in sorted(OUT.iterdir()) if p.is_file() and p.name not in {'files.json','remote-verification.json'}}
 (OUT/'files.json').write_text(json.dumps(files,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(summary,ensure_ascii=False))
if __name__=='__main__':run()
