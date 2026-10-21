# 다양성 확장 사전 점검 — 원작 맵·출현·몸체·아이템 자료

> 후속: [전체 맵의 웹·원본 대조](2026-10-21-full-map-audit/README.md). 아래의 제품 상태와 도장 미조사 표기는 당시 이력이다.

2026-10-20 KST. 기준 `work`의 `8ca52153c0cf2d4f0f6e17259793c92601162d62`. **읽기 전용 조사와 문서 작성**이다. 제품 코드·런타임 에셋·저장 형식·EXE는 변경하지 않았다. 대사 문구·말투·감정 연결은 [별도 대사 분류 문서](2026-10-20-variety-dialogue-catalogue.md)에 둔다. 창작 이벤트 추천은 문서에 넣지 않는다.

## 1. 먼저 확인할 결과

원작에 맵별 출현과 아이템의 다양성을 확보할 데이터는 충분하다. 현재 제품은 그 일부만 표시·실행한다. 우선 원작 ID에 맞는 몸체·그림을 연결해야 추첨 결과가 화면의 다양성으로 이어진다.

| 대상 | 이번에 직접 확인한 원작 데이터 | 현재 제품의 연결 범위 |
| --- | --- | --- |
| 던전 | 일반 정의180개, 요청 범위1,801층 중1,795층 정규화; 참조 오류6층 별도 기록 | 패키지0/15 두 개. 기본 출발은15의7층이며 다음날에도 같은 설정 패키지를 사용한다. 맵 풀 추첨은 없다 |
| 지형 | 유효 층에서 타일셋144개; 층별 날씨·음악·밀도·상점·MH·숨은 계단 필드 | 새2~3방 지형 생성과 선택 패키지의 원작 타일 연결. 원작 지형 생성/날씨/상점/MH 전체 재현은 아니다 |
| 출현 | normal/MH 후보 각각521 MD 레코드. 종·폼·성별 레코드 수이며521종이라는 뜻은 아니다 | 층별 normal 가중치로 논리 ID 추첨; 관계는 제품의 우호1/4·적대3/4 추첨 |
| 몸체 | 위521개 sprite를3개 WAN pack에서 해제·파싱·병합. Walk0/Attack1/Hurt6/Idle7 그룹 존재 | 모든 우호 NPC는 녹색 켈리몬MD383, 모든 적은 보라 켈리몬MD384 몸체로 표시 |
| 아이템 | item_p 1,400레코드, valid1,311개. sprite/palette217조합 전부 이미지 변환 성공 | 바닥에서는 오랭열매70만 원작 그림, 나머지는5×5 표식. 효과는 오랭열매 중심 최소 규칙 |
| 초상화 | KAO1,154행의 기본14감정 존재 여부 조사 | 스타팅34 MD+켈리몬383+노공룡322의36 clip만 현재 감정 패키지에 있다 |
| 고정 장면 | SSA/SSE/SSS1,884개 파싱, 배우 배치7,039행; SSB파일3,760개 존재 확인 | 길드 식사·침실·알람과 던전15의정상 장면을 선별 사용. 전체 이야기 의미/분기는 미분류 |

근거: [감사 요약·원본 SHA256](2026-10-20-variety-data/audit-summary.json). 게임을 실행해 원작 전체를 검증한 결과가 아니라 데이터 파싱·대조 결과다.

## 2. 자료를 찾는 순서

| 자료 | 내용·활용 |
| --- | --- |
| [맵별 목록](2026-10-20-variety-data/MAP_INDEX.md) |180개 정의의 이름·층수·일반 적 후보 MD·현재 우호 처리·타일셋·오류 상태 |
| [맵별 출현 CSV](2026-10-20-variety-data/map-encounters.csv) | 맵×MD별 이름·도감 번호·sprite·normal/MH 출현 층. 층 번호는 해당 구간의1기반 표시 |
| [맵 상세 JSON](2026-10-20-variety-data/maps.json) | 맵별6종 아이템 문맥, 모든 후보 ID, 상점/MH/숨은 계단/밀도/날씨 필드의 값 범위 |
| [층 상세 JSONL.gz](2026-10-20-variety-data/floor-profiles.jsonl.gz) | 유효1,795층의 raw layout·일반/MH 레벨/가중치·special rows·아이템2단계 분포·함정 원시 가중치. gzip 해제 후 한 줄이 한 층 |
| [현재 패키지 대조](2026-10-20-variety-data/runtime-map-comparison.json) |0/15 패키지의 각 층 출현·아이템 비중·타일/그룹 참조가 이번 원작 정규화와 일치 |
| [아이템 CSV](2026-10-20-variety-data/items.csv) / [그림 표본](2026-10-20-variety-data/item-sprites.png) | recordIndex/itemId/이름/category/sprite/palette/valid/실제RGBA hash |
| [몸체 동작 JSON](2026-10-20-variety-data/body-availability.json) |521 MD의 병합 WAN 동작 ID·방향 행·프레임 수. atlas 생성/픽셀/Windows 검증은 별도 |
| [초상화 CSV](2026-10-20-variety-data/portrait-availability.csv) / [원작 감정 표본](2026-10-20-variety-data/original-portrait-moods.png) | MD별 직접 KAO행과 공유 후보,14감정 존재 여부, 현 패키지 연결 여부, 원작 말투 그룹 |
| [원작 장면 배우 CSV](2026-10-20-variety-data/script-actors.csv) | SCRIPT 경로·장소 코드·레이어·actor ID·MD. 장면 문맥을 확인할 때의 진입점 |

이 CSV의 영문 맵/아이템 이름은 SkyTemple 정적 설정 이름이다. 포켓몬 이름은 [고정 pmdsky-debug enum](https://github.com/UsernameFodder/pmdsky-debug/blob/c72fe675919ec83cfe93f687973afae85e00d1a3/headers/types/common/enums.h)이다. 보관된 ROM의 `MESSAGE/text_e.str`는 기존 `pmd2str`로 해독되지 않으므로 실제 패치된 ROM의 한국어 명칭이라고 주장하지 않는다. `Dummy`·`[M:...]`도 정적 이름 그대로 보존했다. 식별의 기준은 이름이 아니라 ROM hash+판본+ID다.

## 3. 맵·층을 연결하는 원본

| 원본 | 참조·보존할 필드 | 구분할 점 |
| --- | --- | --- |
| ARM9 `HardcodedDungeons.get_dungeon_list` | dungeonId, mappa_index, start_after, number_floors, number_floors_in_group | 던전 ID와 MAPPA 그룹 번호는 다르다 |
| `BALANCE/mappa_s.bin` | floor_lists, layout, monsters,6개 item list, traps | `groupFloor=start_after+localFloor`; localFloor는0기반 |
| `DUNGEON/dungeon.bin` | DMA/DPC/DPCI/DPL/DPLA, items.itm.img, traps.trp.img | 타일셋 ID와 pack slot, 청크 ID를 분리한다 |
| `MAP_BG/bg_list.dat`와 BMA/BPC/BPL/BPA | 고정 배경과 팔레트/애니메이션 | 생성 던전의 tileset과 고정 정상/거점 배경은 다른 경로다 |
| `SCRIPT/<level>/*.{ssa,sse,sss,ssb}` | actor 배치·동작·방향·메시지/초상화 명령·장면 전환 | level 코드에서 던전 ID를 문자열 모양만으로 유추하지 않는다 |

해당 ROM의 일반 정의71 `Deep Concealed Ruins`의 local0,173 `[M:D1]Dummy`의 local0~4는 MAPPA 범위를 벗어났다. 임의로 다른 층을 대신 읽지 않았다. **실제 플레이 가능 맵 목록을180개 전체로 간주하면 안 된다.** 도장180~191은 별도 인덱싱이며 이번 일반 정의 감사 범위 밖이다. 고정층·미사용·이야기 전용 정의도 제품 맵 풀과 분리해 선별해야 한다.

완료 장소는 던전15→16 `Steam Cave Peak`만 원작 SSB의 `back_SetDungeonBanner(16,0)`·`D10P41A` 배경과 직접 대조되어 있다. 아래/상층/정상처럼 보이는 연속 ID를 일괄 연결하면 이야기·보스·고정층을 잘못 재생할 수 있다. 나머지는 장면 스크립트·고정층·배경을 확인한 뒤 별도 대응표를 채워야 한다. [기존 완료 장소 근거](../../assets/original/gameplay/completion/provenance.json).

## 4. 맵별 적과 우호 NPC를 어떻게 해석할 것인가

### 현재 지원 맵의 실측 목록

| 맵 | normal 논리 후보 | 현재 우호 NPC/적 표시 |
| --- | --- | --- |
|0 Test Dungeon,3층 |16 PIDGEY,102 EXEGGCUTE,191 SUNKERN,293 WURMPLE,425 CHIMCHAR.102는3F만 | 같은 후보 중 관계 추첨. 우호면383, 적대면384 그림으로 표시 |
|15 Upper Steam Cave,7층 |83 FARFETCHD·193 YANMA는1F;126 MAGMAR는2~7F;237 GRANBULL·240 SHUCKLE·341 VOLBEAT·342 ILLUMISE는1~7F;437 KRICKETUNE는1~3F | 같은 후보 중 관계 추첨. 우호면383, 적대면384 그림으로 표시 |

예를 들어 MD237은 도감237이 아니라 **GRANBULL(도감210)**이다. ID를 도감 번호로 읽으면 다른 포켓몬을 붙이게 된다. 원작 레벨도 이번 자료에 보존했으나 현재5하트/기본공격 계산에 적용되지 않는다.

던전15의 보존 normal 상대 비중은 다음과 같다. 값은 선택 후보 사이의 가중치이며 `%`가 아니다.

| 층 | MD:가중치 |
| --- | --- |
|1F |83:625,193:1042,237:1666,240:834,341:1666,342:1667,437:2500 |
|2~3F |126:1667,237:1666,240:834,341:1666,342:1667,437:2500 |
|4~7F |126:1111,237:1111,240:556,341:1111,342:1111 |

원본CDF의 종료/특수 행을 일반 후보에서 제외하므로 마지막 표의 합은5,000이다. 확률을 표기할 때는 남은 후보 합으로 나눈다. 원본 special rows와 normal/MH 가중치를 함께 남겨 이 처리를 추적할 수 있게 했다.

### 원작 우호 인물의 근거는 세 갈래다

| 근거 | 확인된 데이터 | 이번 분류 |
| --- | --- | --- |
| 일반/MH 출현표 | MD·레벨·가중치 | 일반 적 후보. 원작 우호/적대 플래그가 별도로 붙어 있지는 않다 |
| 던전 상점 | kecleon_shop_chance, shop_items, special 켈리몬 행 | 상점 인물/물품의 원작 근거. 현재의 우호 켈리몬 전체 배치와 동일하지 않다 |
| 고정 장면 | SSA 배우+SSB 문맥, 동료/의뢰/거점의 역할 | 배치만으로 선역·적대·보스·의뢰인을 자동 확정하지 않는다. script-actors.csv에서 원작 장면을 찾아 분기/메시지를 대조한다 |

현재 `EntityKind::Friend`는 역사적으로 **등장 포켓몬 전체를 담는 컨테이너** 이름이다. 실제 적대는 `GameplayState.hostile`의 instance ID로 구분한다. 신규 자료에 `Friend=원작 우호 종`이라고 기록하면 안 된다. 관계·MD·instance를 분리해야 맵별 로컬 주민, 동일 종의 여러 개체, 동료 교체가 충돌하지 않는다.

## 5. 몸체와 초상화 준비 범위

몸체 경로는 `monster.md[MD].sprite_index`→`monster.bin/m_ground.bin/m_attack.bin`의 같은 sprite slot이다. monster/attack은 PKDPX 해제, ground는 raw WAN을 읽어 병합한다. 국가 도감 번호·성별·폼·sprite slot·초상화 KAO행을 같은 숫자로 취급하지 않는다.

이번 출현 후보521개는3pack 파싱/병합 오류가 없고 Walk0/Attack1/Hurt6/Idle7에 모두8개 비어 있지 않은 방향 행이 있다. 그러나 **Nod22는492개, Faint33은498개에서 그룹 자체가 없다.** 전체 종을 스타팅과 같은13동작으로 무조건 export하면 실패한다. 기존 NPC exporter의 명시적 `finite Idle7 응답`·`finite Hurt6 후 제거` 처리처럼 원작에 없는 동작의 대체/연출 생략을 기록해야 한다. 방향 행의 존재는 모든 프레임의 실제 출력/anchor/tick 검증을 대체하지 않는다.

대화용 초상화는 보통 몸체보다 제약이 크다. 원작 뜨거운 동굴 후보 상당수는 기본 얼굴만 있다. 예를 들어 MAGMAR126·YANMA193·VOLBEAT341·KRICKETUNE437는 이번 기본14감정 중Normal만 확인됐다. 현재 패키지에도 아직 해당 종 clip이 없다. NPC 몸체만 추가한 뒤 Friendly cast383을 계속 쓰면 켈리몬 얼굴이 출력된다. **실제 대화 대상 instance→MD→검증된 KAO/감정 clip**으로 연결할 준비가 필요하다.

기존 길드의 몸체도 NPC 확장의 원본 진입점이다.

| 원작 actor ID | 이름 | MD | 현재 몸체 패키지 |
| --- | --- | --- | --- |
|79 |푸크린 |40 |guild/actor-79 |
|131 |페라페 |483 |guild/actor-131 |
|124 |비버니 |434 |guild/actor-124 |
|109 |노공룡 |322 |guild/actor-109 |
|115 |가재군 |369 |guild/actor-115 |
|119 |치렁 |990 |guild/actor-119 |
|101 |해루미 |792 |guild/actor-101 |
|81/82 |디그다/닥트리오 |50/51 |guild/actor-81,actor-82 |
|133 |삐딱구리 |495 |guild/actor-133 |

이 몸체들은 식사/선별 동작용이다. 모든 전투·이벤트 동작이나 대화 초상화가 runtime에 연결되어 있다는 뜻은 아니다. 여성/폼 공유 후보는 초상화 CSV에 따로 적었다. 공유 후보를 검증된 매핑으로 자동 승격하지 않는다.

## 6. 아이템 데이터와 원작 그림

`MAPPA item key`는 **item_p의 recordIndex**다. `item_id` 필드와 모두 같다고 가정하지 않고 둘 다 보존했다. 그림은 `sprite+palette` 조합으로 `DUNGEON/dungeon.bin` slot1022 `items.itm.img`에서 읽는다. 이번1,400레코드가 참조한217조합은 모두 변환됐다. `valid=false`인 기록도 존재하므로 그림이 있다는 이유로 추첨/사용을 허용하지 않는다.

| recordIndex | 정적 이름 | sprite/palette | 현재 효과 연결 |
| --- | --- | --- | --- |
|70 |Oran Berry / 오랭열매 |2/0 |바닥 그림·소지·2하트 회복. 원작 효과 전체 이식은 아니다 |
|81 |Pecha Berry |2/4 |원작 그림 확인; 독/해독 상태 시스템 미연결 |
|73 |Reviver Seed |14/13 |원작 그림 확인; 부활 효과 미연결 |
|87 |Blast Seed |14/13 |원작 그림 확인; 효과 미연결 |
|99 |Max Elixir |12/1 |원작 그림 확인; PP 시스템 미연결 |
|109/110/112 |Apple/Big Apple/Huge Apple |3/6 |같은 그림.109도props에 보관; 현재 식사 사과 표시는 별도 원작 효과624. 크기별 포만감 효과 미연결 |
|183 |Poké |6/3 |원작 그림 확인; 바닥 돈 전용 표시/경제 효과 미연결 |
|1/2 |Stick/Iron Thorn |0/1,0/2 |원작 그림 확인; 투척 행동 미연결 |

아이콘이 같아도 논리 아이템이 같지 않다. 사과 크기·각종 씨앗은 공유 그림을 쓴다. sprite/palette를 아이템 종류 ID로 쓰면 안 된다. 표본 PNG는16px 원작 그림을 확대/재색칠하지 않고 붙인 문서용 접촉 시트다.

6개 목록 `floor_items`, `shop_items`, `monster_house_items`, `buried_items`, `unk_items1`, `unk_items2`를 섞지 않는다. category 선택 후 해당 category의 조건부 item을 뽑고,65535 보장 항목은 랜덤 분포 밖에 둔다. unknown 두 문맥의 의미는 미확정이다. 이름을 임의로 보물/보상으로 바꾸지 않았다. 일반 정의 전체의 양수 후보는 문맥 합집합290레코드지만 특정 맵의 바닥에서290개 모두 나오는 것은 아니다. 문맥별 랜덤 후보 합집합은 바닥233/상점265/MH94/매장27/unknown각213개다. 이번 양수 후보에서valid=false인 레코드는 없었다.

## 7. 원작의 상황 다양성 관련 필드 — 보유/미연결 구분

아래는 새 이벤트 기획안이 아니라 이번 원작에서 읽은 기존 시스템의 데이터 분류다.

| 분류 | 원작에서 찾은 근거 | 현재 연결/남은 확인 |
| --- | --- | --- |
| 맵별 개체/아이템 |normal/MH·6개 item context·레벨·밀도 |normal/floor_items 일부만 사용. 상대 종류의 실제 몸체/초상화 바인딩 필요 |
| 켈리몬 상점 |상점 확률 필드가 양수인 유효 층973개, shop_items와 special 행 |상점 배치/가격/거래/적대 전환 의미 미연결. UI/동작은 별도 작업 |
| 몬스터 하우스 |MH 확률 필드 양수1,223층, 별도 monster/item 후보 |현재 생성·전투 트리거 없음. 표시된 확률 필드만으로 실제 원작 발생률을 검증한 것은 아니다 |
| 함정/이상한 바닥 |25개 trap 가중치, traps.trp.img slot1033; Wonder Tile17 포함 |현재 계단 그림27/28만 선별 사용. trap ID→sprite 연결/효과/상태/재접촉 처리 미연결 |
| 숨은 계단 |hidden_stairs_spawn_chance 양수435층 |설정값 확인. 목적지/비밀방/바자 연결과 조건은 아직 추적하지 않음 |
| 날씨/지형/음악 |raw layout weather/music/darkness/secondary terrain,144타일셋 |원작 팔레트 애니메이션·날씨 효과·환경음/BGM·이동/피해 규칙 별도 확인 |
| 보물/상자/소품 |valid item metadata, 아이템 그림, 거점 SCRIPT object/effect |아이템 의미/개봉/감정 cue를 자동으로 재현하지 않음. 효과는item_p그림만으로 알 수 없음 |
| 의뢰/구조/대화 |메시지 블록·고정 장면 actor/SSB·원작 tbl_talk |분류 자료는 확보. 사건 트리거·분기·보상·발생빈도는 제품 미구현 |

유효 층의98개 profile은 fixed_floor_id가0이 아니다. 수치는 논리 던전/층 기준으로 셌으며 같은 MAPPA층을 공유하는 정의가 포함된다. 실제 원작 엔진의 추가 조건을 적용한 발생 통계가 아니다.

## 8. 구현 전에 더 채워야 할 검증표

1. **맵 허용 목록과 종료 장소:** fixed/미사용/도장 제외 규칙, 맵 이름의 한국어 대응, 정상 배경/SSB 매핑. 원작180개 전체의 즉시 실행을 가정하지 않는다.
2. **출현 개체별 패키지:** 실제 MD의4기본동작과 선택 연출, 원작WAN→atlas 픽셀/anchor/tick 대조, 필요한 NPC만 선택 로드하는 목록.
3. **NPC 역할:** 원작 상점/고정 인물과 제품의 우호 추첨을 구분한 데이터. 이 자료는 맵별 우호 종을 새로 지정하지 않는다.
4. **초상화 결손:** 실제화자/MD별 사용 감정, Normal fallback/보완 출처. 자세한 선택 규칙은 대사 문서를 따른다.
5. **아이템 의미:** valid·문맥·효과 지원 여부, 공유 sprite, 수량/돈/상자/상태/장비의 표시·저장 계약. 그림만 준비된 아이템과 작동하는 아이템을 나눈다.
6. **상황 효과 원본:** 개별 trap/effect ID→pack slot→팔레트/animation 및 원작 호출 SSB/overlay. 현재 효과624 AppleOnHead처럼 직접 호출 근거를 남긴다.
7. **랜덤/저장:** 원작 확률 근거와 제품 빈도를 구분하고, 새 사건·대사 선택이 전투/지도 난수를 바꾸지 않도록 별도 경로·onceKey·복원 기준을 확정한다.

## 9. 조사 방법과 검증 한계

보관 ROM SHA256은 `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`, 판본 식별은EoS_NA/C2SE다. 기존 고정 SkyTemple 파서와 `tools/mdpet/catalogue.py`의 CDF·문맥 정규화를 재사용했다. 전체 원본 경로/해시와 오류는 감사 요약에 있다. 이번 준비물은 모두 `docs/` 안의 자료이며 제품 자산으로 등록하지 않았다.

원본 ROM은 전후 해시가 같다. 현재0/15 패키지는 이번 추출과 출현/바닥 아이템/층 참조를 대조했다. 데이터의 범위·참조·문서 링크·파일 hash를 검토했으며 문서 작업이므로 C++/GUI/전체 CI/EXE 재빌드는 하지 않는다. 파싱된521개 몸체의 전체 그림 출력, 신규 맵의 실제 동작, 원작 사건의 실행 조건, 한국어 원문 복원은 아직 검증되지 않았다.

기존 설명의 상세 근거: [추출 계약](01_DATA_EXTRACTION.md), [원작 이벤트 조사](03_EVENT_BEHAVIOR.md), [출처·고정 커밋](SOURCE_RESEARCH.md), [현재 스타팅 정책](../logs/2026-10-20-starter-union.md). 과거 문서의 미구현 표시는 당시 상태이므로 현재 코드/이번 감사와 구분한다.
