"""실제 EXE의 Wine 장시간 관측. 합성 WTS 해제는 OS 잠금 검증이 아니다."""
import argparse,csv,json,os,subprocess,time,statistics,math
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[3])
p.add_argument('--output',type=Path,required=True)
p.add_argument('--wine',required=True)
p.add_argument('--window-probe',type=Path,required=True)
p.add_argument('--duration',type=int,default=900)
p.add_argument('--analyze-only',action='store_true')
p.add_argument('--dungeon',type=int,default=14)
p.add_argument('--default-launch',action='store_true')
a=p.parse_args();root=a.root.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
def win(path):return 'Z:'+str(path.resolve()).replace('/','\\')
args=[a.wine,win(root/'MysteryDungeonPet.exe'),'--snapshot',win(out/'state.mdes'),'--movement-trace',win(out/'trace.csv'),'--metrics',win(out/'metrics.csv'),'--report',win(out/'error.txt'),'--duration',str(a.duration)]
if not a.default_launch:args+=['--explore',win(root/f'assets/original/exploration/dungeon-{a.dungeon}/exploration.mdpe')]
start=time.monotonic();last=0;helpers=[]
if not a.analyze_only:
 with (out/'process.txt').open('w') as log:
  proc=subprocess.Popen(args,cwd=root,stdout=log,stderr=log)
  while proc.poll() is None:
   elapsed=time.monotonic()-start
   if elapsed>a.duration+45:
    proc.terminate();break
   helper=subprocess.run([a.wine,win(a.window_probe)],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=12)
   helpers.append(helper.returncode)
   if elapsed-last>=30:
    (out/'heartbeat.json').write_text(json.dumps({'elapsed':round(elapsed,2),'running':True,'helper_failures':sum(x!=0 for x in helpers)}))
    last=elapsed
   time.sleep(2)
  code=proc.wait(timeout=15)
 metadata={'default_launch':a.default_launch,'exit':code,'seconds':round(time.monotonic()-start,2),'requested_seconds':a.duration,'helper_failures':sum(x!=0 for x in helpers)}
 (out/'process-result.json').write_text(json.dumps(metadata,indent=2)+'\n')
else:
 metadata=json.loads((out/'process-result.json').read_text())
rows=list(csv.DictReader((out/'metrics.csv').open()))
assert metadata['exit']==0 and rows, {'process':metadata,'reason':'application failed or emitted no metrics; inspect error.txt and process.txt'}
seen={};bundles=set();notices=set();floors=set();actor_directions=set();max_stock=0;count=0
with (out/'trace.csv').open() as f:
 for r in csv.DictReader(f):
  count+=1;key=(r['life'],r['scene_beat']);seen[key]=seen.get(key,0)+1
  if r['dialogue_bundle']:bundles.add(r['dialogue_bundle'])
  notices.add(r['notice_kind']);floors.add(r['floor'])
  max_stock=max(max_stock,int(r['player_oran']),int(r['companion_oran']))
fields=['working_set_bytes','private_bytes','process_handles','gdi_objects','decode_count','render_submissions','cpu_one_core_percent']
warm=[r for r in rows if float(r['elapsed_seconds'])>=60]
def number(r,k):return float(r[k])
stats={k:{'first':number(rows[0],k),'last':number(rows[-1],k),'max':max(number(r,k) for r in rows),'mean':statistics.mean(number(r,k) for r in rows),'p95':sorted(number(r,k) for r in rows)[math.ceil(len(rows)*0.95)-1],'warm_min':min((number(r,k) for r in warm),default=0),'warm_max':max((number(r,k) for r in warm),default=0)} for k in fields}
result={**metadata,'metrics_samples':len(rows),'trace_rows':count,'life_scenes':[{ 'life':k[0],'scene':k[1],'rows':v} for k,v in sorted(seen.items())],'dialogue_bundles':sorted(bundles),'max_held_oran':max_stock,'stats':stats,'observed_floor_values':sorted(floors,key=int),'unavailable_wine_counters':[k for k in ['private_bytes','process_handles','gdi_objects'] if stats[k]['max']==0],'limitations':'Wine10/Xvfb with synthetic WTS unlock; not Windows11 performance or OS power validation'}
(out/'summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False),flush=True)
assert result['exit']==0 and len(rows)>=result['requested_seconds']//2 and max_stock<=1,result
