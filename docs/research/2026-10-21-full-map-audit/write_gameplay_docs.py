"""기존 전체 맵 색인과 연속 원정 문서에서 열리는 맵별 상세 자료를 생성한다."""
from pathlib import Path
from collections import Counter,defaultdict
from fractions import Fraction
import gzip,json,re
OUT=Path(__file__).resolve().parent
EXP=OUT.parent/'2026-10-21-continuous-expeditions'
read=lambda p:json.loads(p.read_text())
def clean(s):return re.sub(r'\[[^\]]*\]','',str(s)).replace('|','\\|')
def pct(value):return f'{float(Fraction(value))*100:.5g}%'
def table(lines,head,rows):
 lines.extend(['','| '+' | '.join(head)+' |','|'+'|'.join('---' for _ in head)+'|'])
 lines.extend('| '+' | '.join(map(str,row))+' |' for row in rows)
 if not rows:lines.append('| '+' | '.join(['해당 원본 행 없음']+['—']*(len(head)-1))+' |')
def run():
 maps=[json.loads(line) for line in gzip.open(OUT/'map-gameplay-details.jsonl.gz','rt')];index=read(OUT/'map-gameplay.json');by_id={x['dungeonId']:x for x in maps}
 routes=read(EXP/'routes.json');links={x['id']:x for x in routes['links']};grounds={x['id']:x for x in read(EXP/'grounds.json')}
 fixed={x['fixedFloorId']:x for x in read(OUT/'gameplay-fixed-spawns.json')};monster={x['mdIndex']:x for x in read(OUT/'gameplay-monsters.json')};items=read(OUT/'gameplay-items.json')
 item_by_id=defaultdict(list)
 for x in items:item_by_id[x['itemId']].append(x)
 rules=read(OUT/'progression-rules.json')['rules'];family={x['group']:x for x in routes['families']}
 (OUT/'maps').mkdir(exist_ok=True)
 conditions={'forward_success':'성공/스토리 진행 조건 충족 후 전진','first_visit_Carnivine_event_pending':'첫 방문 Carnivine 전투 이벤트가 남아 있음',
  'required_unown_stones_missing':'필요 문자돌 부족: 같은 퍼즐 방으로 돌아와 재탐색',
  'required_unown_stones_collected_and_monument_activated':'필요 문자돌 보유·비석 활성화'}
 for m in maps:
  did=m['dungeonId'];sid=m['segmentId'];restricted=m['restrictionsRaw'];myrules=[x for x in rules if did in x['segments']]
  lines=[f"# {sid} · {clean(m['name'])}",'',f"원작 dungeon **{did}** · `{m['symbol']}` · `{m['familyId']}` · MAPPA `{m['mappaGroup']}/{m['startAfter']}` · 유형 `{m['usage']}`.",
   '',f"[전체 ID 색인](../DUNGEON_ID_INDEX.md) · [자료/비율 해석](../GAMEPLAY_DATA.md) · [연속 원정](../../2026-10-21-continuous-expeditions/README.md)",
   '',f"정의 {m['declaredFloors']}층 / 참조 가능 {m['resolvedFloors']}층. 층 표시는 이 구간 안의1기반 순서다. 현재 제품 패키지 지원 여부는 기존 ID 색인에서 확인한다."]
  if m['usage']!='dungeon':lines+=['',f"**주의:** `{m['usage']}` 정의다. 아래 표는 원본 참조 자료이며 독립 일반 탐험 맵으로 채택했다는 뜻이 아니다."]
  if m['missingReferences']:lines+=['',f"**미해결 원본 참조:** `{m['missingReferences']}`. 없는 층은 다른 데이터로 대체하지 않았다."]
  lines+=['','## 진행 방법과 연결', '',f"기본 탐색은 원본 방향 `{'아래층(DOWN)' if restricted['direction']==0 else '위층(UP)'}`의 계단/출구를 따라 구간을 진행한다. 고정 전투·퍼즐·스크립트 이동은 아래 조건을 우선한다.",
   '',f"진행표: {', '.join('`'+x+'`' for x in m['runIds']) or '등록된 다구간 진행표 없음'} / 장기 원정: {', '.join('`'+x+'`' for x in m['arcIds']) or '등록된 장기 원정 없음'}."]
  if myrules:
   lines+=['','특수 진행 규칙: '+', '.join(f"[{x['id']}](../PROGRESSION.md#{x['id'].lower()})" for x in myrules)+'.']
  transition_rows=[]
  for lid in m['outgoingLinkIds']:
   l=links[lid];to=l['toDungeonId'];via=[]
   for gid in l['viaGroundIds']:
    g=grounds[gid];storage=any(x['storageConfirmed'] for x in g['interactions']);statue='저장+창고' if storage else '저장' if g['statueStatus']=='confirmed' else '동상 미확정/연출'
    via.append(f"`{gid}` ({statue})")
   transition_rows.append([f"[{to} · {clean(by_id[to]['name'])}](SEG-D{to:03d}.md)",f"`{lid}`",'<br>'.join(via) or '별도 연결부 미지정',clean(conditions.get(l['condition'],l['condition']))])
  table(lines,['다음 구간','연결 ID','경유 맵 / 기능','조건'],transition_rows)
  if not transition_rows:lines+=['','후속 연결 미등록은 자동 길드 귀환을 뜻하지 않는다. 마지막 보스/보상/귀환·해금 이벤트는 원작 스토리 상태를 따른다.']
  entries=[c for c in routes['connectors'] if c['toDungeonId']==did and c['fromDungeonId'] is None]
  if entries:lines+=['','출발/재시도 ground: '+', '.join('`'+x['groundId']+'`' for x in entries)+'.']
  bs=[b for b in routes['branches'] if b['nextForwardDungeonId']==did or b['viaDungeonId']==did]
  if bs:
   lines+=['','왕복 분기:']
   for b in bs:lines+=['',f"- `{b['fromGroundId']}` → [우회 구간{b['viaDungeonId']}](SEG-D{b['viaDungeonId']:03d}.md) → 같은 ground 복귀. 전진 선택은{b['nextForwardDungeonId']}이며 우회로 완료를 다음 던전 진입으로 바꾸지 않는다."]
  url=family[m['mappaGroup']].get('webUrl')
  if url:lines+=['',f"해금/방문 시점 보조 자료: [원작 지역 문서]({url}). 이 표는 모든 해금 플래그 조합을 실행한 결과가 아니다."]
  lines+=['','## 입장·탐색 제약 원본','', '스크립트의 인원 고정/스토리 조건은 추가 적용될 수 있다. 아래 수치는 현재 제품 규칙이 아니다.']
  table(lines,['항목','원본 값'],[['방향',restricted['direction']],['최대 동행/아이템',f"{restricted['max_party_members']} / {restricted['max_items_allowed']}"],['돈 반입/레벨 초기화',f"{restricted['money_allowed']} / {restricted['level_reset']}"],['모집 허용/경험치',f"{restricted['recruiting_allowed']} / {restricted['enemies_grant_exp']}"],['층 턴 제한/구조 시도 필드',f"{restricted['turn_limit']} / {restricted['max_rescue_attempts']}"],['IQ 비활성화/보물상자 드롭',f"{restricted['iq_skills_disabled']} / {restricted['enemies_can_drop_chests']}"]])
  lines+=['','## 적 출현표','', '비율은 문맥 안 유효 후보의 가중치/합계다. 일반과MH를 분리하며, 고정층 원본 후보는 실제 고정 배치를 대체하지 않는다. MD별 추가 출현 조건/도구 플래그는 [원본 MD 목록](../gameplay-monsters.json)을 따른다.']
  for context,title in [('normal','일반 후보'),('monsterHouse','몬스터하우스 후보')]:
   rows=[e for e in m['enemyRows'] if e['context']==context and not e['fixedFloorOnly']];lines+=['',f'### {title}']
   table(lines,['MD / 원작 식별명','레벨','층','상대 비율','적용 구분'],[[f"{e['mdIndex']} · {clean(e['name'])}",e['level'],e['floors'],f"{e['weight']}/{e['weightTotal']} ({pct(Fraction(e['weight'],e['weightTotal']))})",'고정층 원본 후보' if e['fixedFloorOnly'] else ('MH 생성 후보' if context=='monsterHouse' else '일반 생성 후보')] for e in rows])
  raw_fixed=[e for e in m['enemyRows'] if e['fixedFloorOnly']]
  if raw_fixed:
   lines+=['','<details><summary>고정층 MAPPA 원본 후보 · 실제 고정 적 명단/출현률이 아님</summary>','']
   table(lines,['문맥','MD / 식별명','원본 레벨 필드','층','원본 상대값'],[[e['context'],f"{e['mdIndex']} · {clean(e['name'])}",e['level'],e['floors'],f"{e['weight']}/{e['weightTotal']}"] for e in raw_fixed]);lines+=['','</details>']
  lines+=['','## 고정 전투·아이템 배치','', 'fixed.bin + overlay29의 배치와 overlay10 능력치다. 원본 적/동료 spawn type을 유지하고 대체층을 별도로 표시한다. 대체 적용 조건 전체를 실행 검증한 결과는 아니다.']
  fixed_refs=list(m['fixedFloorIds'])
  for f in m['fixedFloorIds']:
   if fixed[f]['overrideFixedFloorId']:fixed_refs.append(fixed[f]['overrideFixedFloorId'])
  if not fixed_refs:lines+=['','이 구간의 MAPPA 참조에는 고정층이 없다. 스토리 ground에서 일어나는 전투/이벤트의 부재를 뜻하지는 않는다.']
  for fid in dict.fromkeys(fixed_refs):
   fr=fixed[fid];lines+=['',f"### fixed{fid}"+(' (조건부 대체)' if fid not in m['fixedFloorIds'] else ''), '',f"원본 크기 {fr['width']}×{fr['height']}. 대체 fixed ID: {fr['overrideFixedFloorId']} (0이면 등록 없음)."]
   counts=Counter((s['mdIndex'],s['spawnTypeName'],s['statsEntry'],s['statsAppliedByType']) for s in fr['spawns'] if s['mdIndex'])
   rows=[]
   for (mid,typ,st,applied),count in counts.items():
    strow=next(s['stats'] for s in fr['spawns'] if s['mdIndex']==mid and s['statsEntry']==st and s['spawnTypeName']==typ)
    rows.append([f"{mid} · {clean(monster[mid]['name'])}",typ,count,strow['level'] if applied else '템플릿 참조',strow['hp'] if applied else '미확정',f"{strow['attack']}/{strow['special_attack']}/{strow['defense']}/{strow['special_defense']}" if applied else 'spawn type별 별도 처리',st])
   table(lines,['MD / 이름','spawn type','배치 수','레벨','HP','공/특공/방/특방','stats ID'],rows)
   loot=Counter((s['itemId'],s['quantity']) for s in fr['spawns'] if s['itemId'])
   if loot:table(lines,['고정 itemId / 이름','원본 수량 필드','배치 수'],[[f"{itemid} · {' / '.join(dict.fromkeys(clean(x['name']) for x in item_by_id[itemid])) or '이름 미확정'}",qty,count] for (itemid,qty),count in loot.items()])
  lines+=['','## 아이템 후보와 선택 비율','', '비율은 해당 문맥에서 종류→아이템을 선택할 때의 조건부 비율이다. 바닥 드롭률/상점 발생률/보스 보상 확률이 아니다. 범위는 표의 층들 사이 최소~최대이며 [층별 정확한 분수 CSV](../item-floor-rates.csv.gz)와 원본 CDF를 함께 보존했다. 확정 marker를 무작위 비율1로 바꾸지 않았다.']
  for ctx,title in [('floor_items','바닥'),('shop_items','상점'),('monster_house_items','몬스터하우스'),('buried_items','매장'),('unk_items1','의미 미확정 문맥1'),('unk_items2','의미 미확정 문맥2')]:
   rows=[x for x in m['itemRows'] if x['context']==ctx]
   lines+=['',f'<details><summary>{title} · {len(rows)}개 요약 행</summary>','']
   table(lines,['record / itemId','아이템','분류','층','선택 비율 범위 / marker','적용 구분'],[[f"{x['recordIndex']} / {x['itemId']}",clean(x['name']),x['categoryId'],x['floors'],(pct(x['probabilityMin']) if x['probabilityMin']==x['probabilityMax'] else pct(x['probabilityMin'])+' ~ '+pct(x['probabilityMax'])) if x['probabilityMin'] is not None else x['selection'],'고정층 원본 후보' if x['fixedFloorOnly'] else '원본 후보'] for x in rows]);lines+=['','</details>']
  lines+=['','## 층별 환경·발생 설정','', '상점/MH는 원본 chance 필드다. 적/아이템/함정 밀도는 생성 개수 확정값이 아니다. 함정 종류별 원본 가중치와 지형 플래그는 physical profile을 따른다.']
  table(lines,['층','타일 / 고정층','날씨','상점 / MH','적 / 아이템 / 함정 밀도','숨은계단 / 끈적임','physical ID'],[[l['floor'],f"{l['tileset_id']} / {l['fixed_floor_id']}",l['weather'],f"{l['kecleon_shop_chance']} / {l['monster_house_chance']}",f"{l['initial_enemy_density']} / {l['item_density']} / {l['trap_density']}",f"{l['hidden_stairs_spawn_chance']} / {l['sticky_item_chance']}",f"`FLR-G{m['mappaGroup']:03d}-F{m['startAfter']+l['floor']-1:03d}`"] for l in m['layouts']])
  lines+=['','[전체 데이터/해석/재현](../GAMEPLAY_DATA.md) · [출처/추출](../gameplay-extraction.json)','']
  (OUT/'maps'/f'{sid}.md').write_text('\n'.join(lines))
 for row in index:row['progressionRuleIds']=[x['id'] for x in rules if row['dungeonId'] in x['segments']]
 (OUT/'map-gameplay.json').write_text(json.dumps(index,ensure_ascii=False,indent=2)+'\n')
 lines=['# 특수 진행·아이템 요구·원정 경계','', '맵별 상세 페이지와 기존 연속 원정 표에서 사용하는 공통 규칙이다. 원본 배치/후보와 스토리 해석을 구별하며 모든 해금 플래그 조합을 검증한 결과로 표시하지 않는다.']
 for rule in rules:
  lines+=['',f"<a id=\"{rule['id'].lower()}\"></a>",f"## {rule['id']}",'', '적용 구간: '+', '.join(f"[{i}](maps/SEG-D{i:03d}.md)" for i in rule['segments'])+'.','',rule['text']]
  if rule.get('requiredItems'):table(lines,['필요 record ID / 아이템','수량'],[[f"{x['recordIndex']} · {clean(items[x['recordIndex']]['name'])}",x['quantity']] for x in rule['requiredItems']])
  lines+=['',f"[진행 근거]({rule['source']})"]
 (OUT/'PROGRESSION.md').write_text('\n'.join(lines)+'\n')
 # 기존192행 색인의 이름 자체를 상세 페이지 링크로 만든다.
 p=OUT/'DUNGEON_ID_INDEX.md';text=p.read_text();text=re.sub(r'\|\s*(\d+)\s*\|([^|]+)\|',lambda mt:f"|{mt[1]}|[{clean(by_id[int(mt[1])]['symbol'])}](maps/SEG-D{int(mt[1]):03d}.md)|",text)
 if 'GAMEPLAY_DATA.md' not in text:text=text.replace('\n\n| ID','\n\n각 식별명을 누르면 적·아이템·고정 전투·진행/분기·입장 제약을 확인할 수 있다. [표 해석/원본 데이터](GAMEPLAY_DATA.md) · [특수 진행](PROGRESSION.md).\n\n| ID',1)
 p.write_text(text)
 # 기존 맵 개요에 상세 자료 진입점을 넣는다. 재실행할 때 블록을 교체한다.
 overview=OUT/'README.md';body=overview.read_text();start='<!-- gameplay-details:start -->';end='<!-- gameplay-details:end -->'
 block=start+'\n\n## 각 맵의 적·아이템·진행 방법\n\n기존 [192개 ID 색인](DUNGEON_ID_INDEX.md)의 식별명을 누르면 구간별 상세 표를 연다. [자료 해석/층별 원본](GAMEPLAY_DATA.md) · [특수 진행/필요 아이템](PROGRESSION.md). 일반/MH·고정 전투, 아이템6문맥·확률, 제약·환경·연속 원정 조건을 함께 보존했다.\n\n'+end
 if start in body:body=body[:body.index(start)]+block+body[body.index(end)+len(end):]
 else:
  first=body.index('\n');body=body[:first+1]+'\n'+block+'\n'+body[first+1:]
 overview.write_text(body)
 # 구간/진행표/계열별 자료를 동일 SEG ID에 연결한다.
 for segment in routes['segments']:
  did=segment['dungeonId'];segment['gameplayDoc']=f'../2026-10-21-full-map-audit/maps/SEG-D{did:03d}.md'
  segment['gameplayData']='../2026-10-21-full-map-audit/map-gameplay.json'
  segment['progressionRuleIds']=[rule['id'] for rule in rules if did in rule['segments']]
 for collection in ['sectionRoutes','storyArcs','families']:
  for row in routes[collection]:row['gameplayDocs']=[f'../2026-10-21-full-map-audit/maps/SEG-D{did:03d}.md' for did in row['dungeonIds']]
 (EXP/'routes.json').write_text(json.dumps(routes,ensure_ascii=False,indent=2)+'\n')
 # 원정표 숫자 자체도 맵별 문서 링크로 바꾼다.
 doc=EXP/'README.md';body=doc.read_text();outlines=[]
 for line in body.splitlines():
  if line.startswith('| `RUN-D') or line.startswith('| `ARC-'):
   columns=line.split('|');at=3 if line.startswith('| `RUN-D') else 2
   original=re.sub(r'\[(\d+)\]\([^)]*\)',r'\1',columns[at])
   columns[at]=re.sub(r'\b(\d+)\b',lambda mt:f"[{mt[1]}](../2026-10-21-full-map-audit/maps/SEG-D{int(mt[1]):03d}.md)",original)
   line='|'.join(columns)
  outlines.append(line)
 body='\n'.join(outlines)+'\n';marker='<!-- gameplay-crossrefs -->'
 if marker not in body:
  pos=body.index('\n');body=body[:pos+1]+'\n'+marker+'\n각 진행표의 **구간 숫자를 누르면 적·아이템·보스·진행 조건 상세 문서**를 연다. [자료/비율 해석](../2026-10-21-full-map-audit/GAMEPLAY_DATA.md) · [특수 진행/필요 아이템](../2026-10-21-full-map-audit/PROGRESSION.md).\n'+body[pos+1:]
 doc.write_text(body)
 print('map pages',len(maps),'rules',len(rules))
if __name__=='__main__':run()
