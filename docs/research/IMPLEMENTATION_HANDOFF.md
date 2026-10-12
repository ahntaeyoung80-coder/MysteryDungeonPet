# Adaptive Desktop Pet — 구현 명세 작성 인수인계

작성 기준: 2026-10-13 KST. 제품 코드 개발 없이 작성한 기술 결정·연결 계약·검증 기준이다. 후속 Codex는 아래 결정과 원본 소스 목록으로 단계별 명세를 작성하고, 실행이 필요한 gate를 별도 검증 작업으로 배치한다. 전체 조사 완료를 보고하려면 ROM/Windows 검증과 GitHub 반영 상태를 확인해야 한다.

## 1. 확정 기술 결정

| 영역 | 최종안 | 가져오는 코드·데이터 | 최소 신규 구현 |
| --- | --- | --- | --- |
| 원작 데이터 | SkyTemple-files 고정 커밋의 오프라인 파서 | ARM9 던전 정의,MAPPA,MD,item_p,문자열,pack/배경 | 전체 순회·정규화·manifest·참조 검사 wrapper |
| 원작 그림 | ROM WAN를 기본으로 monster/ground/attack merge | CharaWanHandler/시트 export/offset+duration | 선택 atlas packer·support auditor |
| 공개 자산 | SpriteCollab XML/폼 트리/credit | 원본 시트의 구조 및 지원 감사 | ROM↔collab 수동 매핑 규칙을 데이터로 한 번 정의;종별 프레임 수동 편집 없음 |
| 생활 장면 | 원본 SSB/SSA/테이블을 cue 데이터로 정리 | 식사·수면·기상 clip/방향/Wait/배경/소품 | 작은 EventPlayer+Lifecycle;원작 VM 전체 제외 |
| 던전 | 방2~3개+L통로+출발/계단방 | 원작 tileset 및 normal/item 분포 | WorldGenerator/SpawnSampler/flat grid |
| 탐험 | finite 목표 선택+FIFO BFS+경로 캐시 | pmd-bot 정책 참고 | Navigator/Explorer/once 수집/우호적 만남 |
| Windows | C++17/Win32/WIC/PBGRA DIB/UpdateLayeredWindow | Microsoft WIC/DIB 샘플 일부 | WindowHost/Renderer/활동·전원 monitor/StateStore |
| 성능 | 변화와 deadline 기반 update,선택 자산 | integer tick/frame+작은 surface | byte 예산·캐시·Windows 측정 harness |

기술 후보의 모든 실제 경로·함수·커밋은 [SOURCE_RESEARCH.md](SOURCE_RESEARCH.md)에 있다. 데이터·애니메이션 ID 연결은 [ASSET_MAPPING.md](ASSET_MAPPING.md)를 단일 기준으로 사용한다. 각 단계의 상세 자료를 처음부터 다시 조사하지 않는다.

## 2. 추출→변환→runtime 연결

~~~mermaid
flowchart TD
    R["사용자 ROM"] --> P["SkyTemple 오프라인 파서"]
    P --> C["정규화 catalogue"]
    P --> A["WAN·타일·배경 변환"]
    P --> E["SSB·SSA cue 추출"]
    A --> M["선택 asset manifest"]
    E --> M
    C --> N["네이티브 상태·탐험"]
    M --> N
    N --> W["Animator·Windows 렌더"]
~~~

1. ROM을 읽기 전용으로 열고 hash/edition/language를 기록한다. 기존 프로젝트의 제품 소스는 이 조사 작업에서 수정하지 않았다.
2. HardcodedDungeons+MAPPA를 연결하고 localFloor/groupFloor·도장 특례를 정규화한다. MD/item/문자열/tileset을 원본 ID로 연결한다.
3. 누적 spawn 표는 raw 값을 남기고 올바른 context/category에서 상대 정수 가중치로 변환한다. guaranteed와 특별 MD 행은 따로 처리한다.
4. spriteIndex로 3개 MONSTER pack을 읽는다. monster/attack은 압축 해제,ground는 raw WAN. merge→export_sheets로 원본 clip/marker를 확보한다.
5. 필요한 동작만 crop/atlas로 변환하고 tick/방향/anchor/alias/credit을 보존한다. 식사/수면 공간과 소품은 script level→mapid→bg_list 및 object name→GROUND 경로로 연결한다.
6. SSB/SSA는 공식 파서와 SourceMap으로 명령/배치를 추출한다. SETANIMATION_TABLE을 적용한 유한 cue를 만들어 원작 VM에 의존하지 않는다.
7. runtime은 selected catalogue/atlas/manifest/cue만 로드한다. Python/GTK/원작 ROM/전체 SpriteCollab을 설치물에 포함하지 않는다.

## 3. 새 데이터 계약

아래 타입·파일·API는 모두 명세용 신규 제안이다. 현재 저장소에 존재하는 코드처럼 인용하지 않는다. 스키마에는 version을 넣고 양쪽 모듈의 ownership·필드 범위를 고정한다.

| 계약 | 최소 필드·범위 | 생산자 → 소비자 | 불변 조건 |
| --- | --- | --- | --- |
| Provenance | romHash,edition,language,toolCommits,sourcePath/slot,contentHash,license/credit,status | Extractor → 모든 catalogue/assets | 미확인 raw reference는 null/unresolved |
| FloorKey | edition,dungeonId,localFloor | Catalogue → Explorer | localFloor0-based,표시층은+1;groupFloor는 별도 |
| FloorProfile | FloorKey,groupFloor,tilesetId,rawLayout,normalMonsterWeights,itemContexts | Extractor → Generator/Sampler | original distribution과 새 spawn count 정책 분리 |
| PokemonRecord | mdIndex,entid,nationalDex,baseForm,gender,signedSpriteIndex,assetKey | Extractor → Sampler/AssetStore | MD/form 매핑 손실 금지 |
| ItemRecord | recordIndex,itemId,categoryId,sprite,palette,isValid,iconKey,foodKind | Extractor → Inventory/Lifecycle | recordIndex/itemId/categoryId 구분 |
| WeightedList | entries(originalId,rawCDF,relativeWeight),context,guaranteed[] | Normalizer → SpawnSampler | negative weight/invalid refs 오류;sum0이면 빈 목록 |
| WorldGrid | width,height,terrain[],roomId[],start,stairs,version | Generator → Navigator/Renderer | 모든 floor 연결,stairs reachable,출발과 다른 방 |
| Entity/ItemInstance | instanceId,originalId,assetKey,tile,occupancy,friendCooldown | Sampler → Explorer | instance stable,모든 포켓몬 우호적 |
| AnimationRecord | wanId/name,aliasOf,rows,frames(rect,anchor,shadow,markers,ticks),source | Packer → Animator | direction row/marker/crop 같은 원점;tick 정수 유지 |
| EventCue | actor/target,setClip/direction/effect/wait/consume/transition,sourceOffset,rawFlags,onceKey | Cue extractor → EventPlayer | effect slot 확정 전 원작 효과 지원으로 표기 금지 |
| Snapshot | schema/assetVersion,FloorKey,mapSeed,RNG,inventory,life/event phase,onceKeys,savedUTC,window | StateStore ↔ runtime | 포인터/OS handles 저장 금지;중복 소비/획득 금지 |

runtime의 배열/파일 읽기는 byte size·count·offset를 검증한다. 어떤 ID가 u16에 들어간다는 원본 사실과 모든 catalogue 레코드를16bit로 저장해도 된다는 결정은 다르다. 전체 추출 건수를 확인한 뒤 binary width/offset를 확정한다. 조사 단계의 JSON은 인간 검증용으로 사용하고 불필요한 JSON 엔진을 runtime에 추가하지 않는 안을 우선한다.

## 4. 새 모듈 인터페이스와 호출 순서

| 모듈 / 제안 API | 입력 → 출력 | 소유·호출 주기 | 다른 모듈 연결 |
| --- | --- | --- | --- |
| Catalogue.getFloor(FloorKey) | key → immutable FloorProfile | 시작 시 compact data 로드,층 전환 조회 | Generator/Sampler의 유일한 원본 분포 공급 |
| AssetStore.acquire(assetKey) | key → atlas/metadata handle | cache miss decode,LRU 제한,reference 관리 | WIC/PBGRA;Animator/Renderer |
| WorldGenerator.generate(seed,profile,policy) | RNG/선택 프로필 → WorldGrid | 층 진입 시1회 | geometry 검사 실패 시 제한 재생성 |
| SpawnSampler.populate(world,weights,RNG,budget) | 원본 분포+새 개수 정책 → instances,미배치 사유 | 층 진입/명시 spawn 사건 | 가용 타일 비복원 추출,예산·후보 수 제한,소진 시 종료 |
| Navigator.plan(grid,start,goal,occupancy) | tile→path 또는 unreachable | goal/gridVersion/blocked next tile 때만 | Explorer가 캐시 경로 사용 |
| Explorer.nextGoal(world,inventory,life,now) | 현재상태 → goal/action | tile 도착·목표 소실·bounded deadline | 아이템/친구/frontier/계단/귀가 |
| Animator.start/advance(clip,direction,mode,monotonicTime) | clip→currentFrame,markers,finished,nextDeadline | frame 경계와 필요 이동 update | EventPlayer와RenderSnapshot |
| EventPlayer.advance(cue,eventState,now) | cue→clip/effect/consume/finished/deadline | 유한장면/Wait,once transaction | Lifecycle가 이벤트 점유 관리 |
| Lifecycle.update(context,eventResult,utcNow) | 활동/배고픔/수면→Explore/ReturnHome/Eat/Sleep/Wake/Ready | 사건·생활 deadline | Explorer 정지/재개·EventPlayer |
| ActivityMonitor.snapshot | OS hook/messages/idle→activity enum | foreground event,session/power,저빈도 idle | Life 및 render pause |
| Renderer.compose(snapshot) | layers/frames,camera/viewport→PBGRA surface,changed | changed 때만 UpdateLayeredWindow | 표시 영역만 합성,경계 clip,작은 DIB 재사용 |
| WindowHost.dispatch | UI message→drag/DPI/activity/nextWake | message+waitable timer | OS handles RAII,focus 정책 |
| StateStore.save/load | versioned snapshot↔local file | 변화 debounce,suspend/exit save | Inventory once/event/RNG 복원 |

공통 좌표 계약은 [05_WINDOWS_RUNTIME.md](05_WINDOWS_RUNTIME.md)의 좌표·표시 영역 절을 따른다. 던전 이동은 창 내부 카메라로 표시하고 Windows 창 위치는 드래그로 별도 제어한다. 배치 종료 조건과 초과 guaranteed 처리는 [04_DUNGEON_AI.md](04_DUNGEON_AI.md)를 따른다.

기본은 UI thread 하나로 시작한다. 파일 decode가 눈에 띄게 막히면 worker1개를 추가하고 immutable decoded buffer만 전달한다. worker가 HDC/window를 소유하게 만들지 않는다. 실제 측정 없이 thread pool·GPU device·복잡한 VM을 선행 도입하지 않는다.

## 5. 행동 및 원작 장면 연결

탐험은 시작→frontier/item/friend→stairs→다음층→마지막 Pose17→ReturnHome 순이며, 생활은 음식 보유/피로/활동 상태에 따라 Eat15→Sleep5/13/27→Wake14→Ready→Explore를 반복한다. 탐험 시간·음식 소비 정책·친구 cooldown은 신규 정책이다.

확인된 원작 식사 script code64는 WAN15,기상63은WAN14,이벤트수면62는WAN13,클리어26/46/66은WAN17이다. code1/2는 특수 idle 선택이며 표 번호와 WAN ID가 같지 않다. SSA1..8과SSB0..7의 방향 변환을 AssetMapping에만 정의해 복수 구현의 불일치를 막는다.

수면은 반복 clip이므로 finished를 종료 조건으로 기다리지 않는다. 생활 UTC deadline이 깨우기 사건을 내고 Wake는 유한 clip 종료 뒤 재탐험한다. 잠금/화면off/절전 상태는 render pause를 우선하며 자동resume와 사용자 복귀를 구분한다. 재시작 시 이미 지나간 frame/event를 폭주 재생하지 않는다.

획득·음식 소비·클리어는 onceKey가 필요한 상태 변경이다. 저장에 eventPhase와transaction 결과를 함께 넣고 복원 직후 중복 실행을 검사한다. 발견·인사·놀람의 세부 원작 장면은 아직 미고정이므로 지원된 원작 clip+신규 cue라는 사실을 manifest에 남긴다.

## 6. 성능·용량 예산

| 전략 | 적용 위치 | 측정/검증 기준 |
| --- | --- | --- |
| selected assets | 오프라인 packer | 설치된 모든 파일 총합20,000,000bytes 이하. 전 종/폼 지원과 양립 여부는 현재 미측정 |
| frame/alias dedup | packer | 동일픽셀 공유 가능,anchor/ticks는 개별 보존 |
| 작은 surface | Renderer | 200×200×4=160,000bytes/장 계산;실제process RAM과 분리 |
| decoded cache 제한 | AssetStore | 초기8MB 계획 예산,PNG압축 크기와decoded memory 별도 기록 |
| event/deadline update | Animator/Lifecycle/Activity | 같은frame 재제출 없음,lock/off render0,1ms polling 없음 |
| path cache | Navigator/Explorer | small-grid FIFO BFS;goal/version/차단 때만 갱신 |
| 정적 CRT/OS DLL | build/package | release exe 크기/배포DLL 실측;추정0.2~2MB는 성공 기준 아님 |
| resource lifetime | WindowHost/WIC/GDI |100회asset switch/drag/DPI/lock 후 handle 증가·누수 없음 |

CPU는 한 코어 기준과 전체 논리 코어 정규화 기준을 모두 보고한다. RAM은 working set/private bytes/peak를 별도 기록한다. 요구 잠정목표는 CPU평균1% 이하,RAM30,000,000bytes 이하이며 어떤 기준을 acceptance로 채택했는지 명세에 고정한다. 이 조사에서는 성능값을 실제 측정하지 않았다.

## 7. 검증 현황과 개발 전 gate

| Gate | 현재 증거 | 실행해야 할 확인 | 실패 시 처리 |
| --- | --- | --- | --- |
| G1 원작 전체 데이터 | 실제 파서·정규화 규칙 확인 | 사용자 ROM import/전체순회/참조·도장 검증,원본hash 불변 | errors와resume point 기록;unsupported 판본을 지원으로 표기하지 않음 |
| G2 원작 동작 | 2종 XML,8PNG 구조·전체 디코드 확인 | ROM3WAN merge/export,픽셀/alpha/marker/8방향,전체support 감사 | exact/fallback/missing 분리;종별수동프레임편집 대신데이터규칙 |
| G3 이벤트 | 공개8SSB+1SSA 독립 구조 검사,table84entries | 공식 decompile/cue 추출,원작tick wall time,효과642/643/624 및계단graphic | 해결 전 unresolved;효과를 임의원작자산으로지정하지 않음 |
| G4 경량 지도 | Python10,000seeds 통과 | native 연결성,spawn 통계,후보0·초과·guaranteed·중복·종료 검사,blocker/once/저장 | 배치 소진은 종료·사유 기록;지도 실패는 제한retry/귀가 |
| G5 Windows | WIC/Win32 source·API 조건 확인 | 최소창/alpha/drag/DPI/idle/lock/suspend/state 검증 | 실제release harness결과로대안D2D 여부결정 |
| G6 예산 | DIB/grid payload 계산,캐시전략 |15분scenario CPU/WS/private/peak+설치총bytes | 파일/캐시/빈도 최적화;목표 충족 전 완료 주장 금지 |
| G7 원격 보존 | 보고서9개·검사9개·이전명세를main에반영하고원격대조 완료 | 이번 수정은git push 후원격HEAD·파일tree 일치 확인 | push 실패 시완료로표기하지않고미반영커밋 보존 |

문서의 고정 GitHub 코드 경로 86개는 해당 커밋의 tree 또는 실제 파일 응답으로 존재를 확인했다. 이는 모든 함수의 실행·성능 검증을 뜻하지 않는다.

이번 추가 검증: 공개 Pikachu 8PNG는 Pillow 12.3.0 전체 디코딩과 XML 행·열·duration 검사를 통과했다. 고정 원본 calculate_relative_weights 본문을 AST로 분리해 5개 입력의 기대 결과를 확인했다. GUI·ROM 초기화를 실행한 것은 아니다.

이번 실행한 구조 검사의 숫자: SSB8개 모든 고유 routine boundary 통과,SSA286bytes/1layer/12actors/1object,SETANIMATION_TABLE84entries,Python geometry seeds1..10000/2방5000/3방5000/모두통과/최대floor105/계단거리50. 이 숫자는 official ROM파서/게임 실행이나 native성능 결과가 아니다. [04_DUNGEON_AI.md](04_DUNGEON_AI.md)의 검사 코드는 재실행할 수 있다.

## 8. 권장 개발 명세 분할

### 개발 단계 A — 원작 자산과 Windows 기반

범위: 고정 오프라인 환경, 사용자 ROM 판본 검사와 소수 던전/포켓몬 추출, 필수 Walk/Idle/Eat/Sleep/Wake/Pose, 원본 tick/anchor QA, Windows 200px layered 창과 drag/DPI/metric harness. 작은 selected pack을 먼저 만든다.

완료 기준: G1/G2 대표 샘플 통과, 원본 이미지 구분과 누락 상태 명시, 창 투명/상단/focus/drag/DPI 검증, 반복 자원 누수 검사, 성능 baseline과 설치 bytes 기록. 별도 Run은 미확인으로 남기고 fallback을 표시한다. 전체 종/폼은 지원 감사 목록으로 관리한다.

A의 구현 순서:

1. **A-1 자산 준비:** 고정 오프라인 Extractor/Packer로 대표 1~2종의 필수 동작과 선택 catalogue/atlas/manifest를 만든다. 원본 출처·누락을 기록한다.
2. **A-2 렌더러 준비:** A-1과 독립적으로 Win32/WIC/DIB 및 Animator를 같은 데이터 계약으로 준비한다. 명시된 테스트 자산으로 투명도·경계 clipping·DPI·타이머를 검증할 수 있다.
3. **A-3 통합 검증:** A-1/A-2 결과를 연결해 실제 원작 자산으로 필수 동작·anchor·방향·tick과 Windows 기준선을 확인한다. 이 단계가 통과해야 A 완료다.

전체 종·폼 변환 완료를 A-2 착수 조건으로 두지 않는다. ROM 또는 Windows가 없으면 가능한 독립 작업을 수행하고 해당 통합 gate는 미완료로 남긴다.

### 개발 단계 B — 우호적 자율 탐험

범위: native 2~3방과 원작 tileset, normal/item 분포, 출발/계단방, BFS와 frontier/items/friends 목표, 수집 once, 층 전환/Pose/귀가. dungeon-mystery/에뮬레이터/전투 시스템은 의존성에 넣지 않는다.

완료 기준: native 10,000 seeds의 연결·도달 검사, 원본 확률 정규화와 weighted sampling 통계, 차단·재계획·유한 복구, 저장/복원 뒤 획득·클리어 중복 없음. 작은 지도의 spawn 수와 원본 시간당 출현율이 별도임을 명세에 기록한다.

### 개발 단계 C — 생활·활동 연동과 예산

범위: 원본 식사/수면/기상 cue, 친구 교류, foreground/idle/lock/display/power, versioned snapshot과 재시작 복원, 전체 필수 동작 감사, 선택 패키징과 credits, 15분 실측.

완료 기준: 지원된 원본 동작으로 생활 루프, 음식 소비 once, 반복 Sleep의 deadline 종료, 자동 resume와 사용자 복귀 구분, 실제 전원/잠금 테스트, CPU/RAM/설치 목표 달성. 효과/계단/클리어의 세부 자산은 G3가 해결된 것만 exact로 표시한다.

## 9. 저장·업로드 및 재개 지점

현재 전달물은 docs/research/의 요구된 9개 MD이며 ZIP에 같은 폴더 구조로 포함한다. 제품 소스, ROM, 외부 저장소 소스, 게임 이미지, 임시 검사 데이터는 ZIP에 포함하지 않는다. 기존 ahntaeyoung80-coder/MysteryDungeonPet 저장소 루트에 docs 폴더를 추가하면 된다. 새 저장소는 필요 없다.

보고서·검사 자료·이전 명세는 main에 이전 완료했다. 이번 보완 직전 확인한 main은 f52f3b8이다. 현재 저장 상태는 00_RESEARCH_STATE.md와 git log로 확인한다. 최초 조사 ZIP과 checks/package_validation.json의 해시는 수정 전 보고서의 기록이며 최신 문서 해시가 아니다. 과거 ZIP·검사 기록을 현재 문서에 맞추어 덮어쓰지 않는다.

다음 Codex는 먼저 00_RESEARCH_STATE와 이 인수인계를 읽고 미완료 gate를 선택한다. 이미 확인된 파일/함수/테이블을 처음부터 다시 조사하지 말고 고정 링크와 각 phase 보고서의 해당 항목을 참조한다. 원본 ROM과 Windows 실행 환경이 있을 때 필요한 검증을 수행한 후 개발 명세의 완료 조건을 확정한다.
