from pathlib import Path
import os,subprocess,time,csv,json,shutil
root=Path('/project');out=root/'build/lifecycle-gui-fresh';out.mkdir(exist_ok=True)
env=os.environ.copy();env.update(WINEPREFIX='/tmp/mdpet-audit-wine',WINEDEBUG='-all')
def win(path):return 'Z:'+str(path).replace('/','\\')
results=[]
for name,duration in [('solo-night',24),('pair-night',25)]:
 case=out/name;case.mkdir(exist_ok=True);snapshot=case/'state.mdes'
 subprocess.run([str(root/'build/audit/pet_lifecycle_audit_tests'),'--fixtures',str(root/'build/lifecycle-fixtures')],cwd=root,check=True,stdout=subprocess.DEVNULL)
 shutil.copyfile(root/'build/lifecycle-fixtures'/(name+'.mdes'),snapshot)
 args=['/usr/lib/wine/wine64',win(root/'build/audit-win-current/MysteryDungeonPet.exe'),'--snapshot',win(snapshot),'--movement-trace',win(case/'trace.csv'),'--metrics',win(case/'metrics.csv'),'--report',win(case/'error.txt'),'--duration',str(duration)]
 args+=['--explore',win(root/'assets/original/exploration/dungeon-15/exploration.mdpe')]
 if name=='solo-night':args+=['--solo']
 started=time.monotonic()
 with (case/'process.txt').open('w') as stream:
  p=subprocess.Popen(args,env=env,cwd=root,stdout=stream,stderr=stream)
  for i in range(duration+15):
   if p.poll() is not None:break
   subprocess.run(['/usr/lib/wine/wine64',win(root/'build/window-probe.exe')],env=env,cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=10)
   time.sleep(1)
  code=p.wait(timeout=15)
 rows=list(csv.DictReader((case/'trace.csv').open())); submitted=[r for r in rows if r['submitted']=='1']
 bundles=sorted({r['dialogue_bundle'] for r in rows if r['dialogue_bundle']})
 assert code==0 and len(submitted)>10,(name,code,len(submitted))
 assert not any(r['speaker']=='1' for r in rows),'player spoke'
 assert any(r['life']=='0' and r['scene_beat']=='11' for r in rows),(name,'no next-day exploration')
 results.append(dict(case=name,exit=code,seconds=round(time.monotonic()-started,2),submitted=len(submitted),bundles=bundles,lifeScenes=sorted({(r['life'],r['scene_beat']) for r in rows}),images=[p.name for p in case.glob('*.bmp')]))
 print(results[-1],flush=True)
(out/'summary.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
