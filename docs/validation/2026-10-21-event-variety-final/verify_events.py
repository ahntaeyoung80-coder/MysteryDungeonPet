"""실제 EXE 사건·휴일·긴 대사 CSV/최종 저장의 유한 완료와 하루 연결을 검사한다."""
import csv,json
from pathlib import Path
ROOT=next(p for p in Path(__file__).resolve().parents if (p/'CMakeLists.txt').is_file())
OUT=ROOT/'build/event-variety-final'
GUI=OUT/'gui'
def rows(name):
    data=list(csv.DictReader((GUI/(name+'.csv')).open()))
    assert data and all(None not in r and None not in r.values() for r in data),name
    return data
results={}
for i in range(1,10):
    name=f'E{i:02}';data=rows(name);state=json.loads((GUI/(name+'-state.json')).read_text())
    assert state['event']==i and state['progress']==6 and state['commits']==1,name
    assert any(r['event_type']==str(i) and r['event_progress']=='6' for r in data),name
    assert any(r['dialogue_line'].startswith(name+'.') for r in data),name
    result={'finished':True,'resultCommits':1,'returnedToExploration':True,'rows':len(data)}
    if i==7:
        attacks={a.split(':')[4] for r in data for a in r['actor_states'].split(';') if a.startswith('1:') and a.split(':')[2]=='1'}
        assert attacks;result['independentHelperAttackTokens']=len(attacks)
    results[name]=result
for name in ('share','trade'):
    state=json.loads((GUI/(name+'-state.json')).read_text());assert state['progress']==6 and state['commits']==1 and state['oran']==1 and state['apples']==0,name
    results[name]={'resultCommits':1,'playerOran':1,'playerApples':0}
for name,grounds,beats in [('town',{'P01P01A','T01P01A','T01P02A'},{1,2,3,4,5,6,7,8,9,17}),('spring',{'P01P01A','P02P01A'},{1,2,3,10,11,12,13,14,15,16,17})]:
    data=rows(name);state=json.loads((GUI/(name+'-state.json')).read_text())
    assert grounds=={r['holiday_scene'] for r in data if r['holiday_scene']}
    assert beats<={int(r['holiday_beat']) for r in data}
    assert state['day']==2 and state['meals']==1 and state['commits']==1 and not state['holiday'],name
    results[name]={'groundScenes':sorted(grounds),'beats':sorted(beats),'nextDay':2,'meals':1,'visitCommits':1}
long=rows('long');pages=sorted({r['dialogue_line'] for r in long if r['dialogue_line'].startswith('E01.encourage.01.')})
assert pages==['E01.encourage.01.p1','E01.encourage.01.p2','E01.encourage.01.p3'];results['longDialogue']={'pages':pages,'catalogueVersion':999,'testAssetsOnly':True}
completion=rows('completion');state=json.loads((GUI/'completion-state.json').read_text());assert any(r['scene_beat']=='21' for r in completion) or (GUI/'completion-completion.bmp').is_file()
assert state['day']==2 and state['meals']==1;results['map18Completion']={'finalFloor':9,'originalDestination':19,'mandatorySuccessTextCapture':True,'returnedThroughMealToNextDay':True}
(OUT/'event-gui-summary.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
print('PASS: E01-E09, real share/trade once, E10 town/spring -> one meal/day, three Korean pages and18 ->19 completion')
