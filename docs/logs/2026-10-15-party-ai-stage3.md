# 2026-10-15 — 3단계 배우별 전투·MDES8

## 착수·예상

3단계 예상110~180분. 원격 실행물 경로 준비20:53:02~21:05:51 KST, **12분49초**(예상20~30분). [CI38730684329](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38730684329)의 Windows 빌드·native/GUI/설치/루트 EXE 검사, Linux ASan/UBSan·Python, 검증된 실행물의 작업 브랜치 반영이 모두 성공했다. 실제 생성 커밋은 `c09a1dc`다. 관리형 shell 없이 GitHub Actions로 진행한다.

작업 브랜치는 `fix/party-ai-runtime`이며 `work`의2단계 기준선 `51c7cf3`에서 시작했다. `work` 기본 [CI38730332792](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38730332792)는 core/windows 성공이다.

## 첫 구현 — 검증 전

- PartyAction/PartyPlayback과 run/floor/round/actor/action 완료 토큰을 추가했다. 서로 다른 아군의 목표·공격/결과·이동·회복·적의 행동권을 각자 소유한다.
- 아군과 적의 라운드 단계를 분리하되 다른 적의 기절 완료가 살아 있는 적의 행동을 없애지 않게 한다. 이동 후 같은 라운드 추가 공격은 금지한다.
- 공격 시작에 결과/RNG/대상을 저장하고 해당 공격 완료 때 피해를 한번 확정한다. 새 구조에서는 아군 공격도 이 규칙을 사용한다. 기존 단일 조우·구형 저장의 진행 중 공격은 종전 처리 시점을 보존하고 마친 뒤 Explore에서 새 방식으로 전환한다.
- 동료의 전투/복귀/추적 의도를 별도로 저장한다. 포위·목표 없음의 대기는 새 스케줄러에 함께 연결했으며 직선 tie-break·7층·색상 등 후속은 남아 있다.
- MDES8에 액션/플레이백과 길이 검사를 추가하고 MDES1~7 읽기를 유지했다. 잘못된 소유자/중복/확정 결과·피해 journal 불일치·유실된 공격 재생을 거부한다.
- 기존 Animator 캐시로 여러 적의 몸체와 완료를 분리했다. 플레이어/동료/각 적은 자신의 토큰만 내보낸다.
- 구형 snapshot fixture는 새 블록을 제거한 뒤 종전 byte-shape 변환을 적용한다. 새 명시 회귀는 다중 actor event를 읽도록 연결한다.

이 커밋은 첫 통합 컴파일 전의 구현이다. 빌드·회귀·실제 Windows 표시·MDES8 복원과 이전 저장 이행을 통과했다고 보고하지 않는다. 다음은 CI 실패를 확인해 수정하고, keyed 완료·피해/중단 복원 검사를 보강하는 일이다.

## 첫 CI와 수정

`ad2d49d`의 [CI38733826794](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38733826794)는 Windows compile/link와 실제 EXE refresh에 성공했다. native CTest는12/13(19.19초)이며 companion만 `invalid goal tile`로 실패했다. 새 패배 전환에서 탐험 목표를 비우지 않는 경로를 확인해 기존 invalidate 계약을 적용했다. 실패 때문에 Linux/실행물 반영은 실행되지 않았다.

같은 수정에 R10 keyed 완료 토큰의 다른 run/배우/액션·중복 거부와 R11 실제 MDES7 pending-hit의 완료 후 이행을 추가했다. MDES8 형식 문서와 일시정지 clock overflow 검사를 함께 보강한다. 통과 여부는 다음 CI에서 확인한다.

## 두 번째 CI와 진행성 보강

`4fcb808`의 [CI38734363963](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38734363963)는 compile/link 성공, CTest12/13(21.96초)이며 companion의 populated daily loop가 끝나지 않았다. 기존 목표 오류는 지나갔다. 복귀를 완료한 동료가 같은 update에서 접근 불가능한 적을 재선택하면 Follow 상태가 주인공의 다음 차례에 드러나지 않아 통로 교환을 막는 경로가 있다. 복귀 완료 뒤에는 그 차례를 마쳐 다음 주인공 이동에 Follow 상태를 보이게 한다.

실패 시 seed/층/두 위치/목표/행동권/의도/남은 액션·재생을 출력한다. 기본 회귀가 실패해도 명시 후속12검사는 실행·수집하도록 작업 브랜치 CI 진단을 보강했다. 실행물 반영은 필수 Windows/Linux 검사 성공 때만 수행한다.

## 세 번째 CI — 후속9/12 통과·고정 시드 확보

`c936bfd`의 [CI38734980474](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38734980474)는 Windows compile 성공, CTest12/13(18.60초)이다. 후속12개 중9통과/3실패다. 양방향 독립 공격·분리 저장·이동 복원·keyed 완료·MDES7 진행 중 피해 이행이 통과했다. 실패는 직선, HP1 포위, 저체력에서 동료 기존 목표 유지다.

일일 진행성 실패는 seed21/floor1, P78/C102, target=-1,round2469,steps1678로 고정했다. 계단의 일시 실패를 영구 금지하는 경로가 새 배우 점유와 결합할 수 있어 알려진 계단을 각 결정에서 재평가하도록 보강한다. 정확한 계단/HP/실패 횟수도 출력한다.

저체력에서 합법 이웃이 있어도 모두 적 공격권 안이면 안전한 도주 칸이 아니다. 이 경우 임시 probe/자리 교환보다 인접 공격을 선택한다. 진행 중 동료 전투 fixture에는 새 구조의 Engage/target을 명시한다. R04는 각 actor event의 라운드까지 검사하고 출력도 전역 focus 대신 실제 액션 대상을 사용한다.

새 파티 방식에서 목표가 전혀 남지 않았고 실패 기록이 있으면, 일시적인 배우 점유 때문에 제외했던 목표를 한번 다시 계획한다. 이동을 실제로 수행하지 못했고 인접 적은 공격 가능하면 대기 대신 공격으로 전환한다. 기존 단독/구형 전투 경로는 유지한다.

## 네 번째 CI — 후속11/12 통과·일일 진행성 원인 보강

`af20817`의 [CI38735952490](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38735952490)는 compile 성공, CTest12/13(21.62초)이다. 후속은11/12 통과이며 남은 것은 직선 우선이다. seed21은 stairs496/known=-1/HP2,C HP4에서 아직 발견 전인 계단을 향하지 못하고 반복했다. 계단 실패 횟수는0이므로 앞선 영구 계단 실패 가설은 해당 시드의 원인으로 확정하지 않는다.

인접 적이 있어도 Follow 상태의 동료가 주인공 후진 교환에 먼저 소모되면 전투할 차례를 잃을 수 있다. Allies 단계에서 인접 적과 싸울 동료의 의도/목표를 먼저 확보해 그 이동을 강제하지 않게 한다. 적 처치도 실질 진행으로 stalledActions를 초기화한다.

새 독립 이동에서는 우호 NPC를 통과할 때 P/NPC, C/NPC가 각각 합법 자리 교환을 하고 그 이동을 저장한다. 경로 위 미인사 NPC는 기존 인사 흐름으로 연결한다. R12에 인사→통로 통과→아이템 획득·매행동 복원을 추가한다.

## 추가 통합 회귀

후진 자리 교환 전에 인접 적을 상대할 동료 행동권을 확보한다. 우호 NPC가 경로 위에 있으면 인사하고, P/NPC·C/NPC 각각의 합법 자리 교환과 이동 복원을 지원한다. R12로 이 경로의 인사/획득/복원을 검사한다.

Windows에는 `two-sided` fixture와 배우별 clip/action CSV를 추가한다. 같은 제출 프레임의 P/C Attack1·서로 다른 완료 ID, 왼쪽 적 제거 뒤 오른쪽 P Attack1을 요구한다. 일반 Follow의1칸 검사는 유지하고, 독립 전투의 의도된 분리 상태는 native 저장 검증·비중첩으로 검사한다.
