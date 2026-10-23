"""실사용 계측: 상세 프레임 trace 없이 일반 EXE를 관측한다."""
import argparse,json,os,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[3]);p.add_argument('--output',type=Path,required=True)
p.add_argument('--wine',required=True);p.add_argument('--window-probe',type=Path,required=True);p.add_argument('--duration',type=int,default=1800)
a=p.parse_args();root=a.root.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
def win(path):return 'Z:'+str(path.resolve()).replace('/','\\')
logpath=Path(os.environ['WINEPREFIX'])/'drive_c/users/agent/AppData/Local/MysteryDungeonPet/events.log'
offset=logpath.stat().st_size if logpath.exists() else 0
metadata={'wine':True,'requested_seconds':a.duration,'detailed_trace':False,'synthetic_unlock':True}
started=time.monotonic();helpers=[];last_sample=0
args=[a.wine,win(root/'MysteryDungeonPet.exe'),'--snapshot',win(out/'state.mdes'),'--metrics',win(out/'metrics.csv'),'--report',win(out/'error.txt'),'--duration',str(a.duration)]
with (out/'process.txt').open('w') as log,(out/'snapshots.jsonl').open('w') as samples:
 proc=subprocess.Popen(args,cwd=root,stdout=log,stderr=log)
 while proc.poll() is None:
  elapsed=time.monotonic()-started
  if elapsed>a.duration+90:proc.terminate();break
  helpers.append(subprocess.run([a.wine,win(a.window_probe)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=12).returncode)
  inspector=root/'build/quality-release/pet_quality_probe'
  if elapsed-last_sample>=10:
   if inspector.exists() and (out/'state.mdes').exists():
    r=subprocess.run([str(inspector),'--inspect',str(out/'state.mdes')],cwd=root,capture_output=True,text=True,timeout=10)
    if r.returncode==0:samples.write(json.dumps({'elapsed_seconds':elapsed,**json.loads(r.stdout)})+'\n');samples.flush()
    else:(out/'inspection-error.txt').write_text(r.stderr)
   (out/'heartbeat.json').write_text(json.dumps({'elapsed_seconds':round(elapsed,2),'running':True,'helper_failures':sum(x!=0 for x in helpers)}));last_sample=elapsed
  time.sleep(2)
 code=proc.wait(timeout=15)
metadata.update(exit=code,seconds=round(time.monotonic()-started,2),helper_failures=sum(x!=0 for x in helpers))
if logpath.exists():
 with logpath.open('rb') as f:f.seek(offset);(out/'events.log').write_bytes(f.read())
(out/'process-result.json').write_text(json.dumps(metadata,indent=2)+'\n');print(json.dumps(metadata),flush=True)
assert code==0,metadata
