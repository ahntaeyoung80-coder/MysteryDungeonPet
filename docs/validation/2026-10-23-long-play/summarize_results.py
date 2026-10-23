"""중단 전/재개 후 완료 조합을 합치고 중복·누락·실패를 거부한다."""
import argparse,json,itertools,hashlib,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[3])
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();root=a.root.resolve()
def group(names,roots,days):
 rows={};failures=[]
 for name in names:
  for line in (root/'build'/name/'cases.jsonl').read_text().splitlines():
   r=json.loads(line);key=r['case']
   if not r['passed']:
    failures.append(r);continue
   assert key not in rows,('duplicate passing case',key)
   assert r['counts']['days']==days,r
   rows[key]=r
 expected={f'{d}-{m}-{s}' for d,m,s in itertools.product(roots,range(3),range(2))}
 assert set(rows)==expected,{'missing':sorted(expected-set(rows)),'unexpected':sorted(set(rows)-expected)}
 counts={k:(max(r['counts'][k] for r in rows.values()) if k.startswith('max_') else sum(r['counts'][k] for r in rows.values())) for k in next(iter(rows.values()))['counts']}
 assert all(r['case'] in rows for r in failures),'unresolved failed case'
 return {'passed_cases':len(rows),'counts':counts,'cases':list(rows.values()),'resolved_failures':failures}
release=group(['long-play-extended-a','long-play-extended-b','long-play-resumed-a','long-play-resumed-b','long-play-release-repaired'],[1,4,8,14,17,21,35,41],6)
asan=group(['long-play-asan-final','long-play-resumed-asan'],[1,14],2)
gui={name:json.loads((root/'build'/name/'summary.json').read_text()) for name in ['long-play-gui-final','long-play-gui-default-verified']}
for value in gui.values():assert value['exit']==0 and value['requested_seconds']==900 and value['max_held_oran']<=1,value
pe=json.loads((root/'build/long-play-pe-final/summary.json').read_text())
assert pe['failures']==0 and pe['cases']==4,pe
repair=json.loads((root/'build/long-play-event-regression-run/summary.json').read_text())
assert repair['failures']==0 and repair['cases']==1 and repair['totals']['days']==6,repair
result={'movement_repair_asan':repair,'release':release,'asan_ubsan':asan,'windows_pe':pe,'actual_exe':gui,'limits':['Core days are accelerated virtual time, not wall time or a 24-hour stability claim.','Wine10/Xvfb and synthetic WTS unlock; native Windows11, OS sleep/lock/DPI and performance targets are unverified.','Zero private-bytes/handle/GDI counters in Wine are unavailable, not proof of no leaks.','Completed pre-interruption cases used a stricter critical assertion covering all non-player attack events; resumed cases scope it to hostile actors, allowing friendly helper attacks.','attacks counts the representative attack event returned by an update, not every simultaneous actor attack.']}
result['actual_exe_before_movement_fix_sha256']='aae3b0c71a94cc66aff729f67af5150b15a5ed28b71484198b2e06260bd905c5'
result['runtime_sha256']=hashlib.sha256((root/'MysteryDungeonPet.exe').read_bytes()).hexdigest()
result['runtime_build_root_install_equal']=all((root/'MysteryDungeonPet.exe').read_bytes()==(root/p).read_bytes() for p in ['build/playtest-windows/MysteryDungeonPet.exe','build/playtest-install/MysteryDungeonPet.exe'])
assert result['runtime_build_root_install_equal']
result['final_exe_movement_scene']=json.loads((root/'build/long-play-movement-ui/summary.json').read_text())
result['limits'].append('The two 15-minute observations precede the localized routed-event cleanup. The final EXE separately passed restored E06 rendering/return, three related ASan tests and the failing six-day route in Release and ASan; passing long cases were reused.')
result['snapshots']={}
for case in ['long-play-gui-final','long-play-gui-default-verified','long-play-movement-ui']:
 for dungeon in [1,4,8,14,17,21,35,41,15]:
  inspected=subprocess.run([str(root/'build/playtest-asan/pet_variety_scenario'),str(root/'assets/original'),'inspect',str(dungeon),str(root/'build'/case/'state.mdes')],cwd=root,capture_output=True,text=True)
  if inspected.returncode==0:result['snapshots'][case]={'package':dungeon,**json.loads(inspected.stdout)};break
  assert 'snapshot asset version mismatch' in inspected.stderr,inspected.stderr
 else:raise RuntimeError('no compatible snapshot: '+case)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v['counts'] for k,v in [('release',release),('asan',asan)]},ensure_ascii=False))
