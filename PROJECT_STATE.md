# 현재 상태

## 최신 우선 요구 — 길드 생활 장면 (2026-10-13)

사용자가 길드의 주인공 방 취침·길드 식당 식사·길드원 공동 식사 애니메이션을 핵심 요구로 확정했다. 기존 배경+주인공만 표시하던 상태를 보완했다.

- 원본 `SCRIPT/G01P06B/c00a0601.ssa/.ssb`에서 주인공·동료·길드원10명과 식탁 소품107을 확인했다. MDindex/원작spriteIndex/방향/좌표/각각의 scriptAnimationCode를 추출해 `guild.mdgs` cue로 연결했다. 노공룡·비버니·가재군·치렁·페라페·디그다·닥트리오·삐딱구리·해루미·푸크린을 각자의 원작 몸체로 표시한다.
- 식사는8초 유한 장면 동안 원본 지정 Eat15/EventSleep13/Wake14/Walk0을 개별 loop로 재생한다. 모든 길드원에게 Eat15를 적용하지 않는다. 주인공의 식사 소비는 once이며 단일 clip 종료로 공동 식사를 중단하지 않는다. NPC texture는 집 장면 진입 때만 로드하고 캐시한다.
- 원본 `G01P07A/c00a0201.ssa`의 주인공 침상(204,184), Right2와 동료 침상(156,184), Left6을 사용한다. viewport에서는(112,128)/(64,128)이다. EventSleep13→Wake14를 연결하고 두 개체의 기상이 끝난 뒤 Idle·다음날 탐험으로 진행한다.
- 길드원10명 전부 실제 frame 변화 검사 pass. 원본 WAN→merged→atlas516frame/원본996frame 대조 pass, ROM hash 불변. Linux Release4CTests/Python23tests/Windows 크로스 빌드 pass. 기존12시나리오와 저장복원도 재검사했다. Windows Server2025/MSVC CI38399667063의 core/windows 모두success. 실제 GUI에서 식당12명·침실2명·동료Wake 완료 후Ready→day2 Explore를 확인했다. 귀가→식사1522.529ms, 공동식사8177.284ms. 24초 검사 최대working set19,218,432bytes/private7,737,344bytes, decode15회이며 장기 성능 판정으로 확대하지 않는다.
- 원본의 Eating642/EatingSlow643/AppleOnHead624와 Laying27은 provenance에 보존했다. 별도 식사 효과 graphic 연결·원작 대사 디코딩·음악·전체 story VM은 아직 재현하지 않았다. 이를 완전한 원작 연출 이식으로 보고하지 않는다.

구현: `src/core/guild_scene.*`, `src/windows/main.cpp`. 재현: `tools/prepare_gameplay_assets.py`(길드 추출 포함), `tools/prepare_guild_scene.py`. 상세 요구·검증: `docs/GUILD_LIFE_SCENE.md`. 이전 생활 연결 결과는 아래 체크포인트로 보존한다.

## 이전 체크포인트 — B-1 생활 연결·최소 B-2 (2026-10-13)

최초 체크아웃 `/workspace/MysteryDungeonPet`, `work=70eb4cf`는 문서만 있었다. 원격 조회·fetch 후 실제 B-1 코드가 `origin/work=3cbb95b`에 있음을 확인했다. 별도 워크트리 `/workspace/MysteryDungeonPet-b1`, 브랜치 `fix/b1-life-combat`에서 기존 코드·자산을 재사용하고 최신 main의 임시 문제 문서를 병합했다. 기존 체크아웃은 보존했다.

- 구현: 마지막 계단의 once Pose17 → ReturnHome(750ms fade-out + 원작 집 장면 교체 + 750ms fade-in) → Eat15(식사 once) → Sleep5(deadline) → Wake14(유한) → Idle → 다음날 Explore. 귀가·전투 동안 Explorer 이동 및 경로 계산을 차단한다. 패배는 같은 파이프라인에 Hurt6 기반 신규 감정표현 cue를 추가한다.
- 최소 전투: 기본공격355만 사용, 플레이어 선공·3하트, 내부 적중 카운터2, 양쪽 명중80%, 플레이어/적 치명타20%/5%, 피해2배. Attack1/Hurt6/Faint33을 연결했다. 승리는 Faint 완료 뒤 같은 층·위치에서 재개한다. 기술·레벨·적 HP 표시·가방은 추가하지 않았다.
- 조우: 고정된 RNG로 적대/우호3:1, 인접 칸 인식·전투 또는 인사. 오랭열매70은 실제 소지1개로 제한하고, 부족한 체력에서 목표 우선도를 올린다. 전투 중 자동 사용은 텍스트만 표시하며 2하트 회복(최대3) 후 적 턴이다. 기존 inventory는 B-1 획득 감사 기록이며 가방 UI가 아니다.
- 저장: MDES2에 생활·전투 phase/deadline/피해/소지 수/RNG/transaction을 함께 저장한다. MDES1 위치·기존 once 기록을 보존해 업그레이드한다. 상태를 durable commit한 후 화면·텍스트를 공개한다.
- 원작 자산: 보관된 WAN 시트 두 종에서 Attack/Hurt/Faint/Nod/HitGround를 선택하고 원작 G01P06B/G01P07A map을 추출했다. ROM hash 불변. 실행 자산은 `assets/original/gameplay/`. `tools/prepare_gameplay_assets.py`로 새 출력에 재현 가능하다.
- 실행 검증: Linux Release CTest4개, Python23개, Windows x64 MinGW compile/link 통과. 요청된12개 시나리오와20시드×2일,12,031회 상태 복원·24,235 checks 통과. 100,000회 양쪽 공격/2,000개 조우 통계 통과. 초기 checkpoint 강제 kill→복원→Ready 재실행 시 journal 불변 통과. 상세: `docs/validation/2026-10-13-b1-gameplay-scenarios.jsonl`, `2026-10-13-b1-gameplay-kill.json`.
- 추가 검증: ASan/UBSan4CTests와 최신 gameplay 재검사 pass. Windows Server2025/MSVC19.51 CI38393535265의 core/windows jobs 모두 success. 네이티브4CTests와 실제 GUI의 Pose→귀가→Eat→Sleep→Wake→day2 Explore 및 Attack/Hurt/Faint→승리→Explore pass. 귀가→식사 실측1532.942ms. 선택 설치물1,327,754bytes(ROM 제외). 실제 Windows11, OS 잠금·절전·장기 성능은 계속 pending이다.
- 한계: 최소 실행 주기는 수면10초이며 원작 하루 시간 재현이 아니다. 감정표현은 Hurt6을 재사용한 신규 cue다. 적 표시는 선택된 MD4 대표 몸체로 제한하며 전체 출현 종의 몸체 매핑은 pending. 계단·아이템 검증 표시 및 기존 이동 보간 문제는 별도 잔여 작업이다.

재개: `docs/B1_GAMEPLAY_TASK.md`의 완료·미완료 목록과 실행 명령을 확인한다. B-2는 이번 사용자 요구로 최소 범위가 승인됐으며 과거의 선택 단계 승인 대기를 다시 요구하지 않는다.

## 이전 B-1 체크포인트
B-1 비전투 코어·선택 원작 타일 변환 구현 및 Linux/native 검증 통과. 사용자 최신 지시로 `origin/work` 기준 `19ea3a1`을 명시 fetch/fast-forward해 기존 A를 재사용했다. A-3 잔여 gate는 pending, B-1 제품 전체/Windows11 탐험·미연결 원작 그림은 pending, B-2는 미착수다. 사용법은 `docs/B1_USAGE.md`.

이전 A 체크포인트: A-3 착수: 최신 origin/work `3097aa6`의 A-1/A-2와 사용자 Windows11 기록을 재사용해 계측·200px 고정 요구를 구현했다. A-1은 대표 범위 완료, A 전체/B/C는 미완료다. 첨부/공유 원작 package의 native 연결과 MSVC Windows CI의 원작 WIC·단기 재생 로그는 pass다. 실제 사용자 OS 사건·원작 시간/그림자·육안 anchor는 pending이다. 추가 수동 테스트를 요청하지 않는다.

## B-1 구현·검증
- 중단한 초안2개는 `docs/archive/b1-initial-draft/`에 정확한 bytes/SHA-256으로 보존했다. 최신 코드는 `src/core/exploration.*`, `state_store.cpp`, `exploration_package.*`다.
- 24×24/2~3방/L통로/4방향 FIFO BFS, 아이템·친구·미방문·계단 목표와 유한 복구, 비복원·예산 제한 배치, 원작 normal/category/conditional 가중치, inventory/onceKey, 마지막 Pose17/귀가, RNG/목표/점유를 포함한 MDES1 저장·복원 구현.
- A ZIP catalogue와 ROM을 재사용해 dungeon0의3층/tileset0, dungeon15의7층/groupFloor8..14/tileset12를 변환했다. 원작400청크×2/2,304 DMA규칙×2, 기존 MDPA/WIC 재사용. 실행 자산 `assets/original/exploration/`153,968bytes. ROM 전후hash 동일.
- native seeds1..10000 모두pass:2방4,987/3방5,013, 최대floor139/계단거리55. 기존 Python geometry도 별도로10,000pass/2방5,000/3방5,000/105/50. 원본 Python과 과거 조사 검사는 보존했다.
- native8,516,166 checks, fixture1,000run, 원작2던전200run, 매action10,839복원과 원작transaction6,000복원, 잘린snapshot9,654prefix/손상10,000거부pass. 76분포×100,000추첨/최대3.56637sigma, SkyTemple와200지도115,200타일 대조pass.
- Python23tests/Release3CTests/ASan·UBSan3CTests pass. 프로세스 실제kill 후 저장복원·최종 상태/RNG 일치pass. MinGW Windows x64 Release compile/link·선택 설치물4,013,149bytes pass. 크로스 빌드를 실제 Windows 실행으로 판정하지 않는다.
- Windows `--explore`를 기존200px/WIC/Animator/Surface/one-shot timer에 연결했다. 원작타일·몸체캐시, 표시영역합성, 창고정·snapshot commit 후효과공개, render중단 시탐험중단. Windows Server2025/MSVC19.51 CI run38367994286에서3CTests/원작타일WIC·UI이동/창위치고정/200px/decode2회/저장복원과중복방지pass. 실제Windows11·OS사건·장기탐험은pending.
- 원작 아이템/친구/계단 그림은 노랑/파랑/회색 검증표시다. 원작계단unresolved·DPLA/이동보간·전체종/던전·Windows11 탐험/최종성능은pending. 실제 정전·스토리지 장애내구성은미검증이다.

## 기존 A 구현·실행된 검사
- 기존 `tools/mdpet/` Extractor/Packer/CLI와 대표 ROM 검증을 보존. 최신 origin/work `4d6ed6b`를 받아 A-1을 재구현하지 않았다.
- A-1 증거: C2SE/0x271A→EoS_NA, ROM91,394,048bytes/hash `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`, 원본 불변. profile3, MD1/sprite0·MD4/sprite4, 필수6+선택2 clip/348 atlas frames, 원본3 WAN→merge→atlas484 source frames 비교pass, 2회 추출hash 일치. 상세는 기존 A1 로그/summary.
- `src/core/` Package/Animator/Surface, `src/windows/` WIC AssetStore/WindowHost/DIB Renderer/CSV 계측/창 state 구현.
- `tools/compile_runtime_package.py`: A-1 audit→MDPA 소비 형식. alias 자기 tick/flag/marker와 대상 atlas 공유. 합성 A-1→native144 frames 대조pass, 변환 재현성/기존 출력 보호pass.
- Python13 tests pass, C++10,716 checks/전체 잘린 prefix/10,000 seeded mutations pass, ASan/UBSan pass, Windows x64 MinGW Release compile/link pass.
- 직접 생성 fixture/PNG/manifest·재현 검사, MSVC/native WIC와 8개 오류 package의 CI, Windows 실행 ZIP/PowerShell 절차 추가. Windows Server2025/MSVC19.51 Release native core/WIC/PBGRA·cache·8개 오류 package CI pass(run38339188747, 구현680f4c1). 사용자 Windows11에서 WIC/캐시/Hide/100회 전환과 합성 자산 CPU·메모리 기록을 확인했다. 실제 OS 사건의 명시 증거는 별도 미검증.

- 사용자 측정: Walk 한 코어 평균CPU0.1594%, warm-up 후working set최대14,258,176bytes/private2,461,696bytes. Switch100회/decode1회/GDI2개 고정, Hide추가제출0. 상세는 `docs/validation/2026-10-13-a2-user-review.json`.
- A-3 보완: Renderer/CSV 공통 `PlaybackSample`, 방향/동작/frame/장면좌표·tick/flag/anchor/rect 기록, 종료 전 final sample, empty Animator/deadline 중단 구현. 창200px 고정·크기/카메라 조작 메뉴 제거. Linux Release/native10,725 checks·CTests2개·Python18 tests·ASan/UBSan pass. 공유 ROM 재추출도 원본 불변/동일 manifest/484원본프레임 검사pass. 코드8406b2e의work push/원격SHA 일치 확인. Windows CI run38358343599도 MSVC Release/core/WIC/원작native재생/CSV15개 pass(원작10+fixture5). 종료 DPI96/200px 유지, empty marker0과 실제 원작 프레임 진전을 확인했다.

## 자원·지원 한계
사용자 PC는 Windows11로 확인. 연결된 실행기는 Linux6.18/Debian13이며 사용자 PC의 명령 실행/GUI 조작 도구·연결 terminal이 없다. 실제 Windows11 실행은 로컬 Codex 프로젝트 또는 사용자의 스크립트 실행이 필요하다.
사용자가 첨부한 `a1-original-package.zip`을 `local/a3-input/a1-rom-final-01`로 복원했다. 원작2개 asset/348frames RGBA 재조립·MDPA/native 대조와 native128clip/방향 선택/432경계 검사를 pass했다. `local/a3-rom-runtime-01/-02` 변환 결과 동일, 원본219파일 ZIP과 동일. 이후 origin/work `4231a0c`가 사용자 원본 ROM·전체 내부 파일·선택runtime을 assets/original/에 보관했으므로 추가 업로드 없이 사용할 수 있다. 이번 Windows CI에서 원작2개 WIC/cache·native tick·로그10개를 검사했으며 실제 사용자 Windows11 사건·원작 wall time 판정은 별도다.
기존 `text_e.str` unknown byte141은 rawNameHex/actual language=null을 유지하며 unresolved. 전체/도장/종·폼 감사, 원작 wall time, 그림자 표시, 실제 lock/display-off/suspend·DPI변경의 명시 증거와 장기 자원 수명은 pending. 합성 자산의 제출된 성능 기록은 확인했지만 최종 제품 성능으로 확대하지 않는다.

## 다음 실행 지점
1. `docs/B1_USAGE.md`와 `docs/validation/2026-10-13-b1-summary.json`을 읽고 최신work/검증 기록을 확인한다. 사용자 원본 ROM·ZIP catalogue·A코드는 모두 저장소에 있어 재업로드·A재작성부터 요청하지 않는다.
2. `cmake -S . -B build/b1 -DCMAKE_BUILD_TYPE=Release`, build/ctest 후 `pet_explore assets/original/exploration/dungeon-0/exploration.mdpe 42 local/b1-run.mdes`로 저장/복원 코어를 재현한다. 기존 Python geometry는 별도 실행·기록한다.
3. Windows 설치물의 `Run-B1.ps1` 또는 `--explore assets/exploration/dungeon-0/exploration.mdpe`로 기존 원작몸체+타일을 사용한다. 원작 아이템/친구그림·인사cue/계단매핑·DPLA/이동보간을 이어갈 수 있다. 아직 전체완료를 판정하지 않는다.
4. A-3의 원작wall time/anchor/shadow·실제 사용자OS사건은pending 유지. 이전 Windows11/원작CI 기록은 재사용하고 사용자 수동테스트 전체반복을 요청하지 않는다. B-2 전투는 미확정·미착수다.


## 원격 반영
B-1구현f16258d와Windows검사수정3299e3a를work에push하고로컬/원격SHA일치를확인했다. 3299e3a의Linux ASan/UBSan·Python23tests와Windows MSVC/native3CTests·기존A WIC/재생·B-1탐험CI는모두pass. 최종문서증거는별도커밋이며코드를바꾸지않는다. MSVC선택설치886,179bytes/ZIP364,300bytes(ROM제외).

## 기록
`docs/A1_IMPLEMENTATION_HANDOFF.md`, `docs/A1_USAGE.md`, 기존 A1 로그/검증summary, `docs/A2_USAGE.md`, `docs/implementation/MDPA_FORMAT.md`, `docs/logs/2026-10-13-a2-runtime.md`, `docs/validation/2026-10-13-a2-summary.json`, `docs/logs/2026-10-13-a2-user-review.md`, `docs/validation/2026-10-13-a2-user-review.json`.
2026-10-13 사용자 최신 지시로 원본 ROM·전체 파일 입력·검증 패키지의 assets/original/ 커밋을 허용한다. 인증 정보와 임시 local/은 계속 제외한다. 연구 checks/ZIP 보존. push 결과와 실제 Windows 실행 판정을 별도로 기록한다.
