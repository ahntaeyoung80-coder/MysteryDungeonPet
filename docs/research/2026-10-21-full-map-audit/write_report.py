"""검증한 ROM·웹 facts를 전체 목록/차이 표로 정리한다. 원본을 다시 추출하지 않는다."""
from pathlib import Path
import csv,json,hashlib
OUT=Path(__file__).resolve().parent
load=lambda name:json.loads((OUT/name).read_text())
groups=load('route-groups.json');defs=load('dungeon-definitions.json');summary=load('summary.json');web=load('web-reconciled.json')['resolved'];by_name={r['name']:r for r in web}
category={'main-story':'본편 주경로','side-path':'본편 우회로','post-story':'후일담 주경로','optional':'선택 탐험','special':'특별 에피소드','demo':'데모','dojo':'도장'}
compressed=lambda ids:','.join(map(str,ids))
comparisons=[]
for g in groups:
    f=by_name[g['name']];count=f['floorCount'];terminal=sum(d['resolvedFloors'] for d in defs if d['dungeonId'] in g['dungeonIds'] and d['resolvedFloors']==1 and d['fixedFloors']==1)
    if g['category']=='dojo':status='도장12미로는별도5/1/48층 매핑; 단일 페이지 합계와 비교하지 않음'
    elif g['category']=='special' and g['group'] in (73,79,80):status='동명 본편/특별 에피소드 공용 문서; header 층수의 직접 비교 제외'
    elif count==g['resolvedFloors']:status='ROM 유효 구간층 합계 일치'
    elif count==g['resolvedFloors']-terminal:status='별도1층 고정 구간을 제외한 합계와 일치'
    elif count==g['proceduralFloors']:status='비고정층 합계와 일치'
    else:status='차이 유지; 웹 header만으로 보정하지 않음'
    comparisons.append({'group':g['group'],'category':g['category'],'name':g['name'],'ids':compressed(g['dungeonIds']),'romDeclaredFloors':g['declaredFloors'],'romResolvedFloors':g['resolvedFloors'],'nonFixedFloors':g['proceduralFloors'],'fixedFloors':g['fixedFloors'],'webHeaderFloors':count,'comparison':status,'webUrl':f['url']})
with (OUT/'web-rom-comparison.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(comparisons[0]));w.writeheader();w.writerows(comparisons)
(OUT/'web-rom-comparison.json').write_text(json.dumps(comparisons,ensure_ascii=False,indent=2)+'\n')
intro='''# 원작 탐험 맵 전체 대조

사용자 요청 기준일은 2026-10-21이다. 실제 추출·조회 시각은 [summary.json](summary.json)의 UTC를 따른다. 원작 ROM을 새로 읽고 웹의 전체 장소 목록 및 91개 고유 개별 문서를 직접 대조했다. 일부 후보만 추린 앞선 답변을 대체한다. 제품 코드·실행 자산·EXE·출현 설정은 변경하지 않았다.

## 확인한 전체 범위와 집계 단위

**일반 탐험·우회로 69계열, 특별 에피소드 18계열, 데모 6계열, 도장 12미로가 확인된다.** 일반 69계열은 본편 주경로 23 + 우회로 3 + 후일담 주경로 11 + 선택 탐험 32로 나뉜다. 같은 장소의 상층·깊은 곳·정상/보스방은 하나의 계열로 묶고, 특별 에피소드의 동명 장소는 별도 판본으로 구분했다. 합계 105개는 이 기준으로 나눈 루트/미로 항목이며 게임 화면의 공식 독립 던전 총수로 주장하지 않는다.

| 단위 | 확인 수 | 의미 |
| --- | ---: | --- |
| 일반 탐험·우회로 계열 |69|현재 두 구간 외에도 다수의 미연결 원작 장소 존재 |
| 특별 에피소드 계열 |18|같은 이름이라도 본편과 다른 group/타일셋/출현 데이터 |
| 데모 계열 |6|원작 기본 설정에서는Dummy이지만 디버그 enum·웹에서 이름 식별 |
| 도장 미로 |12|타입별10개×5층, Explorer1층, Final48층 |
| named MAPPA group |94|69+18+6+도장1군. 나머지6군은테스트/구조/용도미확정 슬롯 |
| MAPPA 물리 그룹/층 |100 / 1,892|원본mappa_s 전체; 모든 층 구조·출현/아이템/함정 문맥 파싱 |
| 일반 ARM9 정의 |180|보스·중간방·테스트·시스템/이름용 항목도 포함; 실제 장소 수와 다름 |
| 일반 정의의 요청/유효 논리 층 |1,801 / 1,795|범위 오류6개는 임의 대체하지 않고 별도 보존 |
| 도장 추가 층 |99|일반180 정의 밖의 특수 인덱싱을 이번에 포함 |
| restriction/name ID 공간 |256|256개 실제 탐험 맵이라는 뜻이 아님 |
| ROM FAT 파일 / FNT 이름 파일 |8,644 / 8,608|전 파일 크기·SHA256 목록.36개는FNT 이름 없는 파일 |
| 실제 제품 패키지 / 기본 풀 |0·15·18 / 15·18|연결한 범위가 작았던 것이며 다른 맵의 원본이 없는 상태가 아님 |

물리 층 1,892개는 모두 파싱됐으며 오류는 없었다. mappa_gs의 타일셋·고정층 보조표와도 전부 일치했다. 이전에 추출한 논리 층 1,795개의 layout·normal/MH 비중을 다시 대조한 결과도 불일치가 없었다. ROM의 SHA256은 조사 전후 일치했다. 정규 타일셋 125개는 원작 청크·팔레트 이미지 변환과 DMA 참조까지 확인했다. 고정층에서만 사용하는 170 이상 타일 ID 26개는 일반 타일셋 변환 범위와 분리했다.

## 왜 제품에는 두 맵만 있었나

원작 자료가 부족해서가 아니다. 기존 개발에서 **Steam Cave 전체 중 Upper Steam Cave(ID 15)의 7층, Amp Plains 전체 중 Far Amp Plains(ID 18)의 9층만** 실행 패키지·NPC/아이템 그림·완료 장소에 연결했다. 두 장소의 앞 구간인 ID 14(8층)와 ID 17(10층)조차 기본 풀에 없었다. 완료 ground까지 검증한 연결만 사용하던 구현 범위를 원작 후보 전체처럼 좁혀 설명한 것이 잘못이었다.

기존에 ‘해안 동굴’이라고 표시한 패키지 0은 원작의 `Test Dungeon`이다. **실제 Beach Cave는 ID 1의 일반 4층 + ID 2의 고정 1층**이다. 원본 설정 이름과 제품 표시 이름을 구분한다. 이름표 자체는 이번 조회 작업에서 변경하지 않았다.

맵을 제품에 추가하려면 전체 구간·타일셋, NPC 몸체와 얼굴, 아이템 그림과 표시명, 완료 지점, 다음 날 선택, 저장 복원, 실제 EXE를 연결해야 한다. 이번에 확인한 범위는 데이터와 정적 그림이다. 원작 전체의 이야기·지형 생성·보스·날씨·지형 효과를 제품에서 실행 검증한 결과는 아니다.

## 일반 탐험·우회로 69계열 전체

ROM 층 합계는 각 구간 정의의 논리 층을 더한 값이다. 비고정/고정 구분은 MAPPA의 fixed_floor_id를 기준으로 한다. 웹 문서가 보스방·목표방을 포함하는 기준과 다를 수 있으며, 분기와 중간방까지 합친 값이 한 번의 실제 탐험 길이와 같지는 않다. G는 MAPPA group, ID는 dungeon ID다.

| 분류 | G | 장소 | 원작 구간 ID | ROM 유효층(비고정+고정) | 현재 제품 |
| --- | ---: | --- | --- | ---: | --- |
'''
lines=[intro]
for g in groups:
    if g['category'] not in ('main-story','side-path','post-story','optional'):continue
    state='부분 연결: '+compressed(g['packagedIds']) if g['packagedIds'] else '미연결'
    missing=f"; 미해결{g['declaredFloors']-g['resolvedFloors']}" if g['declaredFloors']!=g['resolvedFloors'] else ''
    lines.append(f"|{category[g['category']]}|{g['group']}|[{g['name']}]({g['url']})|{compressed(g['dungeonIds'])}|{g['resolvedFloors']} ({g['proceduralFloors']}+{g['fixedFloors']}){missing}|{state}|\n")
for cats,title in [(('special',),'특별 에피소드18계열 전체'),(('demo',),'데모6계열 전체')]:
    lines.append(f'\n## {title}\n\n| G | 장소 | 원작 구간 ID | ROM 유효층(비고정+고정) | 주의 |\n| ---: | --- | --- | ---: | --- |\n')
    for g in groups:
        if g['category'] not in cats:continue
        f=by_name[g['name']];note='동명 본편과별도group' if g['group'] in (73,79,80) else '원작 설정의Dummy표기를enum/웹으로식별' if g['category']=='demo' else '제품 미연결'
        lines.append(f"|{g['group']}|[{g['name']}]({f['url']})|{compressed(g['dungeonIds'])}|{g['resolvedFloors']} ({g['proceduralFloors']}+{g['fixedFloors']})|{note}|\n")
lines.append('''
## 도장 12미로 전체

도장은 일반 ARM9 정의 표 외의 별도 처리다. [고정SkyTemple의get_dungeon_entry/get_number_floors](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/module.py)와 [Marowak Dojo](https://bulbapedia.bulbagarden.net/wiki/Marowak_Dojo)를 대조했다. 그룹 53의 0~49는 타입별 5층씩, 50은 Explorer 1층, 51~98은 Final 48층이다.

| ID | 원작 명칭 | 층수 | MAPPA group53 범위 |
| ---: | --- | ---: | --- |
''')
for d in defs:
    if d['dungeonId']>=180:lines.append(f"|{d['dungeonId']}|{d['configName']}|{d['resolvedFloors']}|{d['startAfter']}~{d['startAfter']+d['resolvedFloors']-1}|\n")
lines.append('''
## 구간·더미·별칭을 잘못 세지 않기

- ID 0은 Test Dungeon이다. ID 175~178의 이름은 Shaymin Village / Armaldo’s Shelter / Luminous Spring / Hot Spring이지만, ARM9 정의에서는 group 79의 층을 재참조한다. 이를 추가 탐험 맵 4개로 자동 편입하면 안 된다. 실제 거점·온천 ground는 별도 MAP_BG·SCRIPT 경로를 사용한다.
- ID 105/106, 171~173은 용도가 확정되지 않은 Dummy 정의다. 층 자료가 있어도 웹에서 확인한 정식 플레이 루트로 분류하지 않았다. **ID 165~170은 이들과 달리 데모 6종으로 식별됐다.**
- ID 71은 group 35의 존재하지 않는 30번 층, ID 173은 group 99의 존재하지 않는 5~9번 층을 가리킨다. 다른 정상 층을 복사해 오류를 숨기지 않았다.
- ID 192~211의 도장 계열 예약·별칭과 ID 212~255의 구조·합류 출처·기타 메타 ID는 별도 물리 층 정의로 세지 않았다. [256ID전체표](all-256-identities.csv), [실제일반/도장192항목](DUNGEON_ID_INDEX.md)을보존한다.
- 일반·도장 정의가 직접 참조하지 않는 물리 층 13개(group 52의 19번, group 78의 4~15번)도 확인했다. 미참조만으로 미사용을 확정하지 않았다.

## 웹과 원본의 차이

[94계열 전체 대조 CSV](web-rom-comparison.csv)에 웹 header와 ROM 전체·비고정·고정 층을 함께 기록했다. Steam Cave 15층과 Amp Plains 19층은 별도 정상방 1층을 뺀 값이며 ROM 정의 합계는 각각 16·20층이다. Miracle Sea 22층과 Spacial Rift 24층도 별도 고정 1층 구간을 제외한 범위와 맞는다. 반면 Beach Cave의 웹 표기 5층은 보스방을 포함한 4+1이다. 이를 강제로 같은 기준으로 고치지 않았다.

Drenched Bluff는 ROM에서 6층인데 MDFW header는 7이며, Bulbapedia도 header 6 / 본문 7이 혼재한다. 이견을 남기고 원본 층수 6을 보존했다. Star Cave / Temporal Tower / Dusk Forest는 본편과 특별 에피소드가 동명 문서에 섞일 수 있어 header만으로 SE 층수를 보정하지 않았다. 마스터 목록의 총수와 범주 합도 기준이 불명확하므로 그 숫자를 그대로 복사하지 않고 집계 단위를 명시했다.

전체 웹 목록과 고유 개별 문서 91개를 조회했다. 최초 timeout 3개는 재조회했고, 데모 6개의 MDFW 개별 namespace 404는 Bulbapedia 동명 문서로 보충했다. shell의 직접 HTTP는 환경 proxy의 403으로 막혀 웹 전용 도구를 사용했다. [첫시도](web-facts.json), [재조회/최종근거](web-reconciled.json).

## 그림과 재현 자료

- [93계열 원작타일 표본](all-named-route-samples.png): 도장을 제외한 일반·SE·데모의 정적 청크 표본이다. 생성한 탐험 지도 스크린샷은 아니다.
- [정규타일셋125개전체](all-regular-tilesets.png), [픽셀hash/DMA변환기록](tile-validation.json).
- [ROM파일8,644개 manifest](rom-files.csv), [MAPPA물리1,892층](physical-floors.csv), [정의/참조](dungeon-definitions.json), [요약](summary.json).
- 재현:저장소루트에서 `PYTHONPATH=. local/p1-venv/bin/python docs/research/2026-10-21-full-map-audit/audit_rom.py`, `preview_tiles.py`, `write_report.py`를같은폴더경로로실행한다. 웹facts는조회시점의추출메타데이터를사용한다.

원작 이름은 고정 디버그 enum / SkyTemple 정적 설정과 웹 명칭을 사용했다. 패치 ROM의 한국어 문자열을 새로 해독한 결과는 아니다. 원본 그림의 권리는 Nintendo / Pokémon / CHUNSOFT에 있다. 문서·데이터·그림은 제품용 asset 폴더에 추가하지 않았다.

## 주요 출처

- [MDFW 전체 장소 목록](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:List_of_Locations)
- [Bulbapedia 위치 분류](https://bulbapedia.bulbagarden.net/wiki/Category:Pok%C3%A9mon_Mystery_Dungeon:_Explorers_of_Sky_locations)
- [Serebii Sky 던전 안내](https://www.serebii.net/dungeonsky/dungeon/):웹페이지의안내번호를ROM dungeon ID와혼동하지않는다.
- [고정 pmdsky-debug ID/group 정의](https://github.com/UsernameFodder/pmdsky-debug/blob/c72fe675919ec83cfe93f687973afae85e00d1a3/headers/types/common/enums.h)
- [고정SkyTemple 일반/도장처리](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/module.py)
''')
(OUT/'README.md').write_text(''.join(lines))
lines=['# 일반/도장 dungeon ID192항목 전체\n\nROM일반정의180개+도장12개의실제매핑이다.플레이가능장소192개라는뜻이아니다. [분류/원본/웹전체대조](README.md).\n\n| ID | 식별명 | 정적 설정명 | group/offset | 요청/유효층 | 고정층 | 현재 패키지 |\n| ---: | --- | --- | --- | ---: | ---: | --- |\n']
for d in defs:lines.append(f"|{d['dungeonId']}|{d['symbol']}|{d['configName']}|{d['group']}/{d['startAfter']}|{d['declaredFloors']}/{d['resolvedFloors']}|{d['fixedFloors']}|{'있음' if d['hasRuntimePackage'] else '없음'}|\n")
(OUT/'DUNGEON_ID_INDEX.md').write_text(''.join(lines))
print('generated complete69+18+6+12 tables,192ID index and94web/ROM comparisons')
