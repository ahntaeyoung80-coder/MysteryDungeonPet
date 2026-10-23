"""실사용 자원 관측과 자연 탐험의 흐름/난이도 측정을 분리해 요약한다."""
import argparse,csv,json,math,statistics
from pathlib import Path
ROOTS=(1,4,8,14,17,21,35,41)
NAMES={1:'해안 동굴',4:'삐죽삐죽 산',8:'길드 대원정',14:'열수의 동굴',17:'일렉트릭 평원',21:'유사 동굴',35:'바닷가 동굴',41:'시한의 탑'}
def p95(values):return sorted(values)[math.ceil(len(values)*.95)-1] if values else None
def resources(path,wine=False,warmup=60):
 with Path(path).open(encoding='utf-8-sig') as source:
  rows=list(csv.DictReader(source))
 if len(rows)<2:raise ValueError('at least two metric samples required')
 keys=('elapsed_seconds','cpu_one_core_percent','working_set_bytes','private_bytes','decode_count','render_submissions','process_handles','gdi_objects','render_paused')
 rows=[{k:float(r[k]) for k in keys} for r in rows]
 cpu=[];weights=[];gaps=[];flat=0;max_flat=0
 for prev,row in zip(rows,rows[1:]):
  dt=row['elapsed_seconds']-prev['elapsed_seconds']
  if dt<=0:raise ValueError('metric time did not increase')
  if row['render_submissions']<prev['render_submissions']:raise ValueError('render counter reset inside one run')
  if dt>5:gaps.append({'after_seconds':prev['elapsed_seconds'],'gap_seconds':dt})
  active=not prev['render_paused'] and not row['render_paused'] and dt<=5
  if active and prev['elapsed_seconds']>=warmup:cpu.append(row['cpu_one_core_percent']);weights.append(dt)
  flat=flat+dt if active and row['render_submissions']==prev['render_submissions'] else 0
  max_flat=max(max_flat,flat)
 steady=[r for r in rows if r['elapsed_seconds']>=warmup]
 first=[r for r in steady if r['elapsed_seconds']<warmup+60];last=[r for r in steady if r['elapsed_seconds']>=rows[-1]['elapsed_seconds']-60]
 counters={}
 for key in ('working_set_bytes','private_bytes','process_handles','gdi_objects','decode_count'):
  values=[r[key] for r in steady]
  counters[key]={'max':max(values,default=None),'first_window_mean':statistics.mean(r[key] for r in first) if first else None,'last_window_mean':statistics.mean(r[key] for r in last) if last else None,'last':rows[-1][key],'available':max(r[key] for r in rows)>0}
 average=sum(c*w for c,w in zip(cpu,weights))/sum(weights) if weights else None
 targets={key:None for key in ('cpu_one_core_1_percent','working_set_30MB','private_30MB')}
 if not wine:
  targets['cpu_one_core_1_percent']=average<=1 if average is not None else None
  for key,source in [('working_set_30MB','working_set_bytes'),('private_30MB','private_bytes')]:targets[key]=counters[source]['max']<=30_000_000 if counters[source]['available'] and counters[source]['max'] is not None else None
 return {'environment':'wine' if wine else 'native_windows','duration_seconds':rows[-1]['elapsed_seconds'],'samples':len(rows),'warmup_seconds':warmup,'active_cpu_observed_seconds':sum(weights),'cpu_one_core_average_percent':average,'cpu_one_core_p95_percent':p95(cpu),'cpu_one_core_max_percent':max(cpu,default=None),'counters':counters,'sample_gaps_over_5_seconds':gaps,'max_unpaused_no_submission_seconds':max_flat,'performance_targets':targets,'target_scope':'native Windows required' if wine else 'observed native process only; manual OS checks are separate'}
def profiles(path):
 rows=[json.loads(line) for line in Path(path).read_text(encoding='utf-8').splitlines()]
 expected={(r,False,s) for r in ROOTS for s in (41,7,42,99)}|{(r,True,41) for r in ROOTS}
 identities=[(r['root'],r['solo'],r['seed']) for r in rows]
 if len(set(identities))!=len(rows) or set(identities)!=expected:raise ValueError('quality profile matrix incomplete or duplicated')
 result=[]
 for root in ROOTS:
  team=[r for r in rows if r['root']==root and not r['solo']];solo=next(r for r in rows if r['root']==root and r['solo'])
  reached=[r for r in team if any(f['boss'] for f in r['floors'])]
  lost_at_boss=[r for r in reached if not r['completed'] and r['floors'][-1]['boss']]
  result.append({'root':root,'name':NAMES[root],'floors':team[0]['total_floors'],'team_samples':len(team),'team_clears':sum(r['completed'] for r in team),'team_boss_arrivals':len(reached),'team_defeats_in_boss_room':len(lost_at_boss),'solo_seed41_clear':solo['completed'],'approximate_virtual_seconds_range':[min(r['approximate_virtual_ms'] for r in team)/1000,max(r['approximate_virtual_ms'] for r in team)/1000],'max_revisited_moves_without_progress':max(r['max_moves_without_progress'] for r in team),'max_quiet_update_ms':max(r['max_unexplained_idle_ms'] for r in team),'berries_used':sum(r['berries_used'] for r in team)})
 return {'cases':len(rows),'team_clears':sum(r['completed'] for r in rows if not r['solo']),'team_samples':32,'solo_clears':sum(r['completed'] for r in rows if r['solo']),'solo_samples':8,'routes':result,'scope':'virtual 250ms clock, finite completions after 600ms; these are not rendered wall times or population win probabilities'}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profiles',type=Path);p.add_argument('--metrics',type=Path);p.add_argument('--wine',action='store_true');p.add_argument('--output',type=Path,required=True)
 a=p.parse_args();s={}
 if a.profiles:s['gameplay']=profiles(a.profiles)
 if a.metrics:s['resources']=resources(a.metrics,a.wine)
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(s,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps(s,ensure_ascii=False))
if __name__=='__main__':main()
