"""완료된 파티 행동이 있는 E06 저장을 최종 EXE로 복원해 실제 이동/반환을 확인한다."""
import argparse,csv,json,os,shutil,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[3]);p.add_argument('--output',type=Path,required=True)
p.add_argument('--wine',required=True);p.add_argument('--window-probe',type=Path,required=True)
a=p.parse_args();root=a.root.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
def win(path):return 'Z:'+str(path.resolve()).replace('/','\\')
with (out/'pe-tests.txt').open('w') as log:
 subprocess.run([a.wine,win(root/'build/playtest-windows/pet_event_movement_tests.exe'),'--fixtures',win(out/'fixtures')],cwd=root,stdout=log,stderr=log,check=True)
shutil.copyfile(out/'fixtures/movement-event.mdes',out/'state.mdes')
args=[a.wine,win(root/'MysteryDungeonPet.exe'),'--snapshot',win(out/'state.mdes'),'--explore',win(root/'assets/original/exploration/dungeon-15/exploration.mdpe'),'--duration','20','--movement-trace',win(out/'trace.csv'),'--metrics',win(out/'metrics.csv'),'--report',win(out/'error.txt')]
start=time.monotonic()
with (out/'process.txt').open('w') as log:
 proc=subprocess.Popen(args,cwd=root,stdout=log,stderr=log)
 while proc.poll() is None and time.monotonic()-start<45:
  subprocess.run([a.wine,win(a.window_probe)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=12)
  time.sleep(1.5)
 code=proc.wait(timeout=10)
rows=list(csv.DictReader((out/'trace.csv').open()))
result={'exit':code,'seconds':round(time.monotonic()-start,2),'rows':len(rows),'event_states':sorted({(r['event_type'],r['event_progress']) for r in rows}),'bundles':sorted({r['dialogue_bundle'] for r in rows if r['dialogue_bundle']}),'images':[p.name for p in out.glob('*.bmp')]}
(out/'summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
assert code==0 and any(r['event_type']=='6' and r['event_progress']=='6' for r in rows),result
