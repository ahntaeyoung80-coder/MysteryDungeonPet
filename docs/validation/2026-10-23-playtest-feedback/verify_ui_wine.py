import os,subprocess,time,json,csv
from pathlib import Path
import argparse
parser=argparse.ArgumentParser(description='Wine-only rendered UI checks; synthetic WTS unlock is not a native OS test.')
parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[3])
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--wine',required=True)
parser.add_argument('--window-probe',type=Path,required=True)
parser.add_argument('--exe',type=Path)
parser.add_argument('--fixture-tool',type=Path)
parser.add_argument('--duration',type=int,default=9)
config=parser.parse_args()
root=config.root.resolve();out=config.output.resolve();out.mkdir(parents=True,exist_ok=True)
wine=config.wine;tool=str(config.window_probe.resolve())
exe=config.exe.resolve() if config.exe else root/'MysteryDungeonPet.exe'
fixture=config.fixture_tool.resolve() if config.fixture_tool else root/'build/playtest-asan/pet_variety_scenario'
env=os.environ.copy()
def win(p):return 'Z:'+str(p.resolve()).replace('/','\\')
results=[]
for name in ['status','morning','alarm','wounded-npc','pickup-tm','pickup-orb','heal','default-new','default-restore']:
 case=out/name;case.mkdir(exist_ok=True);snapshot=case/'state.mdes'
 if name=='default-restore':snapshot=out/'default-new/state.mdes'
 elif name!='default-new':
  subprocess.run([str(fixture),str(root/'assets/original'),name,'15',str(snapshot)],check=True,cwd=root,stdout=subprocess.DEVNULL)
 args=[wine,win(exe),'--snapshot',win(snapshot),'--movement-trace',win(case/'trace.csv'),'--metrics',win(case/'metrics.csv'),'--report',win(case/'error.txt'),'--duration',str(config.duration)]
 if name not in ['default-new','default-restore']:args+=['--explore',win(root/'assets/original/exploration/dungeon-15/exploration.mdpe')]
 started=time.monotonic()
 with (case/'process.txt').open('w') as stream:
  process=subprocess.Popen(args,cwd=root,env=env,stdout=stream,stderr=stream)
  helpers=[]
  for i in range(max(4,int(config.duration/2))):
   if process.poll() is not None:break
   r=subprocess.run([wine,win(Path(tool))],cwd=root,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=15);helpers.append(r.returncode)
   if process.poll() is None:time.sleep(1.6)
  try:code=process.wait(timeout=15)
  except subprocess.TimeoutExpired:
   subprocess.run([wine,win(Path(tool)),'close'],env=env,cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=15)
   try:code=process.wait(timeout=10)
   except subprocess.TimeoutExpired:process.kill();code=-999
 rows=list(csv.DictReader((case/'trace.csv').open())) if (case/'trace.csv').exists() else []
 metrics=list(csv.DictReader((case/'metrics.csv').open())) if (case/'metrics.csv').exists() else []
 item={'case':name,'exit':code,'seconds':round(time.monotonic()-started,2),'traceRows':len(rows),'submissions':max([int(r.get('render_submissions',0)) for r in metrics] or [0]),'helpers':helpers,'portraits':sorted({(r['speaker'],r['emotion'],r['line_id'],r['notice_kind']) for r in rows}),'images':[p.name for p in case.glob('*.bmp')]}
 results.append(item);print(json.dumps(item,ensure_ascii=False),flush=True)
 (out/'summary.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
