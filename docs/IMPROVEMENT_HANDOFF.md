# 개선 제작 인수인계

## 현재 재개 기준 — 대각선 반복·Reset 완료, 2026-10-15

`work`의 사용자 보고 적2↔4/플레이어6↔8 반복을 재현하고 수정했다. 동반 역주행 재계획, MDES6 최근32개 상태 반복 감지/같은 층 우회, 백업 후 우클릭 `Reset 탐험 (귀가)`가 최신 구현이다. MDES1~5를 읽고 확정 HP·획득품·journal을 보존한다. 최신 work pull 후 루트 EXE를 실행한다.

반복 감지 후 자동 귀가는 사용자 정정으로 폐기했다. 최신 구현1babcf2의 CI38692398718은 core/windows 성공이다. Linux10/10·Python26/26·Windows native11/11, 실제 앱77→54 우회·같은 run/층의 재실행·자동 귀가0회·수동 Reset과 기존 이동31시나리오·생활/전투/루트 EXE 실행·복원을 통과했다. [최신 검증 요약](validation/2026-10-15-detour-summary.json)을 따른다. 최신 우회는 기존 다음 칸/직전 칸을 피하고 같은 상태 재등장 시 후보를 순환한다. 출구가 없으면 대기한다. [최신 명세](NAVIGATION_DETOUR_SPEC.md)와 PROJECT_STATE.md를 따른다.

이전 자동 귀가 구현605c300의 CI38674804212 core/windows 모두 성공. Linux ASan/UBSan10/10·Python26/26, Windows native11/11·원래 왕복 탈출/자동 복구/메뉴 Reset/백업/중복/재실행과 기존 이동31시나리오·생활/전투/루트 EXE 실행·복원이 통과했다. [원인·사용법](NAVIGATION_RECOVERY.md), [검증 요약](validation/2026-10-15-navigation-summary.json), PROJECT_STATE.md를 우선한다. 새 보고는 snapshot과 `.reset-backup`부터 보존한다. 사용자 Windows11 화면·OS 사건·장기 성능은 별도 미검증이다.

## 이전 이동·카메라 검증 — 2026-10-15

개발 기준은 `work`다. 이동·카메라 안정화의5단계 작업을 완료했다. 기존 이동·카메라 결함은 재현되지 않았고 고정 snapshot·실제 합성/제출 trace와 회귀 검사를 추가했다. CI38666625639 core/windows 성공, Release9/9·ASan/UBSan9/9·Python26/26·Windows native10/10, 고정31시나리오·2,362합성frame·2,055제출·이동 중 kill/복원과 기존 생활/전투/루트 EXE 실행이 통과했다. [재현 안내](MOVEMENT_VALIDATION.md)와 [검증 요약](validation/2026-10-15-movement-summary.json)을 우선한다.

루트 EXE와 소스/자산 hash가 일치한다. 다음 독립 범위는 실제 사용자 Windows11 육안·OS 사건·장기 성능이며, 이번 검사를 대신 반복할 필요는 없다. 새 체감 문제가 보고되면 저장·증거를 보존하고 해당 시나리오만 재현한다. 임시 문제 문서는 사용자 화면의 과거 체감 항목이 남아 있어 보존한다. 아래 내용은 이전 체크포인트 이력이다.

## 후속 수정 — 기상·생존·자리 교환

최신 기준은 PROJECT_STATE.md의 기상·생존·자리 교환 수정과 [MDES5](implementation/MDES5_FORMAT.md)다. 동료 취침/기상/인사를 복원하고 주인공이 침상 아래 바닥을 거쳐 왼쪽 통로로 나간다. 열매 손실2칸 기준·HP2 이하 생존 목표·발견 순서·아이템1턴·우호 NPC 원자적 교환을 적용했다. 루트 EXE도 갱신했다. 구현6680f4d의 CI38521173193은 core/windows 모두success이며 MSVC CTest7/7·침실2명/기상/인사/이탈 GUI·루트 EXE 다른 CWD 실행/복원/진단을 통과했다. 다음 확인은 사용자 Windows11 육안·OS 사건·장기 성능이다. 검증은 docs/validation/2026-10-14-survival-summary.json이며 아래 주인공만 취침/기상·아래 방향 이탈·MDES4 설명은 이전 이력이다.

## 최신 인수인계 — 즉시 구현 명세

이번 기준은 `PROJECT_STATE.md`의 즉시 구현 명세 반영과 [MDES4](implementation/MDES4_FORMAT.md)다. 소스/기존 자산과 함께 루트 EXE를 커밋하며 ZIP은 만들지 않는다. 실행은 [Windows 안내](WINDOWS_QUICKSTART.md)를 따른다. 목표 정책·층 fade·밤 취침·주인공만 기상/이탈·식사7초를 코어 회귀에 포함했다. 구현5643e8e의 Windows CI38517206000와 Linux CI가 success다. MSVC CTest6/6·생활/전투 GUI·루트 EXE 다른 CWD 무인자 실행/복원/진단을 통과했다. 다음 확인은 사용자 Windows11의 육안·OS 사건·장기 성능이다. 아래 계단 최우선·두 배우 기상·ZIP 설명은 이전 이력이다.

## 최종 체크포인트 — 2026-10-14

사용자 확정 명세의 단계0~7 구현과 이 환경에서 가능한 회귀 검사를 완료했다. 브랜치는 `work`, 마지막 구현 커밋은 `97cfb8b`이다. 원본 ROM·압축본은 기준 SHA256 그대로이며 Push는 하지 않았다. 새 코드의 실제 Windows GUI·OS 사건·장기 성능 검증은 남아 있다.

| 단계 | 로컬 커밋 | 완료 범위 |
| --- | --- | --- |
| 0 | a791336 | 최신 생활·전투 포함 기준 확보 |
| 1 | 84d0777 | 8방향 공통 판정·보간·방향 위상·카메라 |
| 2 | f2dc3a3 | 계단 기억·frontier·즉시 재계획 |
| 3 | fba113f | 소비 기반 순서·적 추적·8방향 공격 |
| 4 | 85e3ada | HP5·배우별 완료·Faint·검정 화면 |
| 5 | 3563d55 | 원본 계단·열매·노란 원반·5하트 확대 |
| 6 | c80048d | 전체 식당·낮밤 침실·취침/기상·NPC 대화 |
| 7 | 97cfb8b | MDES3 표시 복원·구버전 호환·통합 회귀 |

최종 검증: Release CTest **5/5**(24.58초), ASan/UBSan CTest **5/5**(272.24초), Python **25/25**(20.091초, skip 없음), Windows x64 MinGW compile/link, 모든 PowerShell 검사 스크립트 문법 통과. 원본 비교로 소품 재추출 bytes·전체 배경·밤 하늘306픽셀 이외의 보존도 확인했다. 로그와 배포물 hash는 [검증 요약](validation/2026-10-14-improvement-summary.json)에 남긴다. 크로스 빌드·문법 검사는 실제 Windows 실행 증거가 아니다.

실행 ZIP은 작업 환경의 `local/MysteryDungeonPet-improved-windows.zip`이다. 압축을 모두 풀고 EXE를 실행하면 기본 자동 탐험을 시작한다. 실행 파일·파생 자산·기존 실행 안내만 포함하며 원본 ROM·압축본과 저장 파일은 포함하지 않는다.

## 확정 범위
2026-10-13 사용자 명세: P0 8방향 연속 이동·카메라·계단 우선, P1 소비 기반 턴·추적·8방향 공격·HP5·사망, P2 원본 소품·식당 이동·취침/기상·NPC 대화·하트 확대. 단계0~7별 검증·로컬 커밋. Push는 별도 지시 전 하지 않는다.

## 기준 확보 (단계0)
- 기존 브랜치 work / HEAD70eb4cf, 미커밋 변경 없음.
- 원격 work3cbb95b, 최신 생활·전투 fa659271. 기존 HEAD는 최신의 조상이므로 브랜치 전환·reset 없이 work를 fa659271로 fast-forward했다.
- ROM SHA256: 6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf
- rom-files.zip SHA256: cc9d113ea4b438d8b8895a30c3d47863dc99cdb1e1381ce8c029aeeb8d748b54
- 원본 두 파일은 수정 금지. 기존 C++ 코어·GUI·자산 준비 도구 재사용.
- 환경: Linux/GCC14, cmake·Python 고정 의존성 준비. Windows11 직접 실행 불가. 새 코드는 교차 컴파일과 코어 회귀로 검증하고 실제 Windows 결과와 구분한다.

## 다음 작업

구현 재시작 지점은 없다. 다음 확인은 Windows에서 새 코드의 네이티브 빌드·실제 GUI 회귀다. 저장소 루트에서 `powershell -ExecutionPolicy Bypass -File tools/run_windows_checks.ps1`로 기존 검사와 갱신한 생활/전투/기본 실행 검사를 수행한다. 생활 검사는 새 fade·Nod 순서를 포함하도록 실행40초·마감60초로 확장했다. 결과는 이번 Linux 검사와 별도로 기록한다.

육안 확인 대상: 8방향 연속 전환과 1~3배 카메라 끝단, 계단 기억·도착, 적 추적/공격과 배우 중첩, Faint→검정 문구→귀가, 전체 식당 이동, 밤 창틀 보존, 두 배우 Wake/Nod, NPC 발화/응답 뒤 표시 유지. OS 잠금·절전·정지/재개 및 장기 성능은 별도 확인한다. 문제가 재현되면 `src/windows/main.cpp`의 `advanceExploration()`, `restoreMovement()`, `currentPlayback()`, `advanceGuildActors()`, `render()`부터 확인한다. 논리/호환 재현은 `tests/improvement_tests.cpp`, `tests/gameplay_tests.cpp`, `tests/fixtures/`를 사용한다. `docs/TEMP_NEXT_SESSION_ISSUES.md`는 실제 화면 확인이 남아 있어 유지했다.

원작 참고는 고정 커밋 fc850408의 요청된 이동·카메라·시야·턴·AI 코드만 국소적으로 사용했다. 이미 확보한 원본 패키지를 우선 재사용했으며 전체 ARM 엔진·특수 이동 능력·속도 단계·상태이상·전체 기술 시스템은 추가하지 않았다.

## 단계1 결과
- 원작 방향0~7·8방향 BFS(대각선 한 행동), 지형 코너와 도착 타일 점유 분리. 이진 일반 바닥에서 이동/공격 모두 직교 양옆 벽 차단.
- Motion 공통 표시 구간(actor/action/from/to/monotonic/duration/direction), 24틱/400ms, pause/resume. Windows 플레이어·카메라·표시 deadline 연결. 적 이동은 단계3에서 같은 모듈로 연결한다.
- 보행 방향 변경은 Animator 시작 시각/marker 상태 유지. 픽셀 위치는 표시 때만 반올림.
- 기준선 CTest4개 pass, 변경 후 CTest5개 pass(29.05s), Windows x64 MinGW compile/link pass. 실제 Windows GUI의 부드러움은 아직 미실행이다.
- 기준 MDES1/2 fixture는 기존 writer로 생성하여 후속 호환 검증에 보존했다.

## 단계2 결과
- 발견/실방문 mask·방문 방·계단 기억·목표 실패/정체 저장. 같은 방과 지형 경로2행동 주변을 발견(원작 room/corridor 모델의 이진 맵 적용).
- 알려진 도달 가능 계단 > 발견 영역 경계 > 보조 목표. 경계로 접근하여 새 시야를 확보하므로 미방문 전체 순회 제거. 점유로 막혀도 계단 기억 유지.
- 경로 다음 타일 재검사·즉시 재계획, 목표 정리 자체는 이동 행동/시간 미소비. 실제 계단 도착은 이전 목표와 무관하게 층/클리어 처리. NPC는 인접 목표로 접근.
- MDES3 writer·MDES1/2 reader. 기본 누락 필드는 방문 기록에서 복원하며 과거 점유 타일 목표는 재계획. 호환 fixture 검증.
- CTest5개 pass, 10,000 geometry seed·1,000 탐험·원작 던전 회귀 pass. Windows x64 compile/link pass.
- 이전 '모든 아이템 수집', '열매가 항상 목표 최우선' 검사는 사용자 계단 우선 명세에 맞게 변경했다. blocked frontier 앞의 발견 경계 접근으로 조기 귀가 문제를 수정했다.

## 단계3 결과
- actionRound/playerActionRound 및 각 배우 actionRound로 플레이어→적 instance 순서를 처리. 이동/대기/공격/열매 사용은 소비, 방향은 비소비. 적 이동400ms 완료는 추가 공격권을 만들지 않는다.
- player(3,3)→(4,3), enemy(5,3) 고정: EnemyTurn. player(3,3) 고정, enemy(5,3)→(4,3): PlayerTurn. 양쪽 player(3,3)→(4,3), enemy(6,3)→(5,3) 소비: 다음 round PlayerTurn. 원작은 플레이어 이동 직후 인접이면 적 공격 가능하며 거리 선공 예외는 추가하지 않았다.
- Idle/Chase·마지막 발견·목표·막힘·방향·배우별 잔여 적중수 저장. 방/통로 발견 시8방향 BFS로 추적하고 도착 점유 검사. 막힘3회 이후 추적 종료/대기. 이동은 공통 Motion·보행 Animator 연결.
- 공통 canAttack의 8방향·지형 코너 사용. 기존 명중/치명타/피해/열매 유지.
- CTest5개 pass(18.07s): 세 소비 상황·대각선 추적 좌표·이동 중 완료 신호 무시·MDES2 안전 기본값. Windows x64 compile/link pass. GUI에서 새 추적 실측은 미실행.

## 단계4 결과
- 공통 kMaxHp=5로 기본값·기상 회복·열매 상한(회복량2 유지)·저장 검증·UI 개수 통일. 과거 저장의 현재 HP는 그대로 유지.
- enum 끝에 PlayerFaint/Death 추가(기존 번호 불변). 피격 완료→플레이어 Faint33→숨김→750ms fade-out→1000ms 불투명 검정/한국어 문구→귀가 장면750ms fade-in→기존 패배 처리. 화면 유지 시간은 구현 선택이다.
- ActorCompletion(player/enemy/partner) 분리. Faint는 플레이어 실제 완료만, Wake는 두 배우 완료를 기다린다. 기존 bool API는 headless 시뮬레이션의 활성 배우 완료로만 변환한다.
- CTest5개 pass(16.09s), Windows x64 compile/link pass. HP 기본5·다른 배우 완료 차단·사망 순서·검정 RGBA alpha255·저장복원 검증. GUI 문구/연출 실측은 미실행.

## 단계5 결과
- prepare_dungeon_props.py: slot1022 sprite2 palette0 열매16×16, slot1033 sprite27/28 palette1 계단24×24. palette index0(각16색 bank 첫색) 투명 유지.
- 원본995의 50개4bpp tile table에서 노란 중형 원반 tile8/9,997 palette0를 조합한16×8·shadowSize1. 원본 픽셀 그대로, frame.shadow-anchor 발 위치에 먼저 그린다. 모든 우호 배우에 표시하는 것은 사용자 정책이다.
- 동일 카메라·보간 좌표로 계단/열매/원반 합성. 하트5개2배. 기존 미확인 일반 아이템은 명시적 검증표시 유지.
- 두 번 추출한 전체 props 파일 bytes 일치, ROM hash 불변. CTest5개 pass·원본 크기·하트 픽셀 개수 검증, Windows x64 compile/link pass. 원본 ROM/압축본은 변경하지 않았다.

## 단계6 결과
- 식당504×360·침실 낮/밤384×312 전체 원본 배경과 SSA 월드 좌표로 전환. MDGS2에 방 크기 저장(MDGS1 읽기 유지). 기존 배우 패키지 bytes 변경 없음.
- 식당 카메라 중심368,212→168,212(4초는 구현 선택). 원본 공동 식사8초·소비once 유지, 식사 시작/종료 fade. 배경·모든 배우·소품에 같은 camera.
- 식당 fade-out→낮 침실 fade-in→마주보는 Nod22(두 배우)→원본 Laying27 자세500ms 유지→fade-out→밤배경 fade-in→EventSleep13→두 배우 Wake14 완료→Nod22→fade-out→다음날 초기화/회복once→탐험 fade-in.
- 원본G01P07C 창 내부(148,96)-(190,133)의 확인된 하늘 RGB(7,31,95)306픽셀만 검정. 창틀/창살·다른 원본 픽셀 보존. 원본 데이터 수정 없음.
- NPC 인접 이동 종료→마주보기→플레이어/NPC Nod와1200ms씩 한국어 발화→완료once→Explore. 대화 동안 이동/재접촉 잠금, 완료해도 우호NPC 표시·점유 유지. 대사는 신규 한국어 정책이며 tbl_talk.tlk 원문 복원 아님.
- CTest5개 pass(16.73s): 모든 생활 subphase와 두 배우 완료, NPC 시작/응답/완료·복원. Windows x64 compile/link pass. 원본 두 파일 SHA256은 기준과 동일. 실제 Windows 화면·밤 창틀 육안 확인은 미실행.

## 단계7 결과

- MDES3에 플레이어/배우 MoveRecord와 보행 시작 시각을 저장한다. 재실행 시 남은 이동과 보행 위상을 복원하고, 정지/재개 시 이동·애니메이션·장면 타이머를 함께 보정한다. 기존 취침의 UTC 마감은 유지한다. 단일 UTC 읽기와 상한으로 표시 마감의 underflow를 막는다.
- MDES1·2 fixture는 이전 writer 기반이다. 기존 완료 NPC의 비표시를 유지하고, 미완료 접촉 저장의 겹친 NPC만 가장 가까운 비점유 바닥으로 이동한다. 플레이어 위치·RNG·완료 기록은 유지한다. 기존 Defeat 저장은 피해를 재적용하지 않고 플레이어 Faint로 이어진다. 새 NPC는 대화 완료 후에도 표시·점유를 저장한다.
- 층 교체 시 이동 표시를 초기화하고 새 행동 순서를 시작한다. 계단이 발견되면 incidental 우호 접촉이 우선순위를 가로채지 않는다. 계단/frontier 이후 보조 목표에서는 기존 부족 HP의 열매 우선·회복량2·HP5 상한을 유지한다.
- 적이 시야를 잃으면 마지막 발견 타일 자체까지 이동한 뒤 Idle로 돌아간다. 3배 확대의 홀수 viewport에서도 카메라 기준점을 고정해 번갈아 바뀌는 픽셀을 막았다. 아침 Nod 완료 자세는 fade 중 유지한다.
- 회귀는 세 선공 좌표·중복 행동 차단, 모든8방향 보행 위상, 프레임 간격별400ms·pause·끝단, 계단 기억/막힘, 배우별 사망/생활 완료, 전 단계별 저장 재실행, 구버전 접촉/완료 NPC·손상 저장, 원본 자산 픽셀을 포함한다. 상세 형식은 [MDES3_FORMAT.md](implementation/MDES3_FORMAT.md)다.

## 원작·사용자 정책·구현 선택의 구분

- 원작 확인: 방향0~7, 일반 보행의 지형 코너 판정, 정상 이동24틱, 기존 WAN의 Faint33/Nod22/Laying27/EventSleep13/Wake14, 지정 슬롯의 소품·팔레트, SSA 월드 좌표·전체 배경. 현재 종 Laying은1프레임 자세 그대로다.
- 사용자 정책: 계단 우선/frontier, 5HP·5개2배 하트, 양측 이동 소비 상태의 다음 플레이어 기회, 모든 우호 배우의 노란 원반, 새 한국어 NPC 대사.
- 구현 선택: 기존 이진 맵에서 방+지형 경로2행동 시야, 소비 round의 최소 순차 처리, 대표 적 몸체·기존2회 적중 상태, 카메라 이동4초·fade750ms·검정 문구1000ms·Laying500ms·NPC 각1200ms·수면10초. 원작 전체 시간/AI/생활 엔진 재현으로 보고하지 않는다.
