"""새로 허용한 맵/사건 조합의 실제 EXE 제출·글자·행동을 확인한다."""
from pathlib import Path
import csv,json
from PIL import Image
root=Path(__file__).resolve().parents[3]
source=root/'build/all-map-events/gui';out=Path(__file__).parent
cases={'E03-0':3,'E04-0':4,'E08-0':8,'E03-15':3,'E04-15':4,'E08-15':8,'E05-18':5}
report={}
for case,event in cases.items():
    rows=list(csv.DictReader((source/(case+'.csv')).open()))
    shown=[r for r in rows if r['submitted']=='1' and r['event_type']==str(event)]
    assert shown,(case,'event not submitted')
    frames=sorted(source.glob(case+f'-event-{event}-*.bmp'))
    found=[]
    for frame in frames:
        im=Image.open(frame).convert('RGB')
        ink=sum(min(im.getpixel((x,y)))>210 for x in range(16,184) for y in range(152,184))
        if ink>40:found.append((frame,ink))
    assert found,(case,'no visible dialogue')
    state=json.loads((source/(case+'-state.json')).read_text())
    record={'event':event,'submittedRows':len(shown),'dialogueCaptures':len(found),'state':state}
    if event==4:
        attacks=[r for r in shown if r['event_progress']=='3' and (r['clip']=='1' or r['companion_clip']=='1' or any(part.split(':')[2]=='1' for part in r['actor_states'].split(';') if part))]
        assert attacks,(case,'MH did not reach actual attack playback')
        record['attackRows']=len(attacks)
    elif event==8:
        assert state['progress']==6 and state['commits']==1,(case,'warp did not finish actual reunion once')
        record['actualReunionCommittedOnce']=True
    elif event==5:
        assert state['progress']==6 and state['commits']==1,(case,'scenery did not finish once')
    frame,ink=found[0];Image.open(frame).save(out/(case+'.png'));record['sampleWhiteInkPixels']=ink
    report[case]=record
(out/'gui-summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(report,ensure_ascii=False))
