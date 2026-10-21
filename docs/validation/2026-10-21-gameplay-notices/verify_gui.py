"""최종 EXE의 실제 제출 화면/CSV를 검증한다."""
import csv,json
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[3]
source=root/'build/notices-final/gui'
report={}
expected={'pickup':(1,70),'pickup-comment':(3,70),'pickup-apple':(1,109),'pickup-tm':(1,188),'ko':(2,0)}
for case,(kind,item) in expected.items():
    rows=list(csv.DictReader((source/(case+'.csv')).open()))
    selected=[r for r in rows if int(r['notice_kind'])==kind and int(r['notice_item'])==item and int(r['submitted'])]
    assert selected,(case,'missing submitted notice')
    path=source/f'{case}-notice-{kind}-{item}.bmp'
    im=Image.open(path).convert('RGB')
    ink=sum(min(im.getpixel((x,y)))>210 for x in range(16,184) for y in range(152,184))
    assert ink>40,(case,'missing GDI text')
    if kind==3:assert all(r['speaker']=='2' for r in selected),'comment portrait is not partner'
    else:assert all(r['speaker']=='0' for r in selected),'system notice has portrait'
    assert len({r['notice_deadline_ms'] for r in selected})==1,(case,'notice restarted during movement')
    assert len({r['player_tile'] for r in selected})>1,(case,'notice blocked exploration')
    report[case]={'kind':kind,'item':item,'submittedRows':len(selected),'whiteInkPixels':ink,'playerMovedDuringNotice':True}
rows=list(csv.DictReader((source/'heal.csv').open()))
assert rows and all(r['player_hp']=='5' and r['companion_hp']=='5' for r in rows),'full heal split into partial ticks'
assert (source/'heal-full-heal.bmp').exists()
report['heal']={'playerHp':5,'companionHp':5,'allObservedFramesFullyHealed':True}
(Path(__file__).parent/'gui-summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(report,ensure_ascii=False))
