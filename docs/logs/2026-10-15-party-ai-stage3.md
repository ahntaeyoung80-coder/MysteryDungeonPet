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

## 다섯 번째 CI — 후속12/13 통과·가시성/예산 경로 수정

`d7b37c6`의 [CI38736964942](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38736964942)는 compile 성공, CTest12/13(22.81초), 후속12/13 통과다. R12의 우호 NPC 통로 통과도 통과했다. seed21의 known=-1·stalled1676 상태는 남아 있었다.

새 planPlayer는 우회 moveTo 경로에서 기존 scheduleDungeon의 discover 호출을 빠뜨렸다. 모든 플레이어 결정 전에 가시성을 갱신한다. 기존 selectPriority의 예산 만료는 모든 목표 선택을 껐으므로, 자동 귀가를 금지한 새 방식에서는 영구 대기를 만들 수 있다. 새 독립 파티는 매 결정의 유한 검색을 유지하되 이 카운터로 목표를 영구 금지하지 않고, 진단 카운터는 기존 저장 상한에서 포화시킨다. 기존 단독·구형 경로는 유지한다.

R13은 반복 복구의 방 가시성 갱신, R14는 옛 예산을 소진한 저장에서 도달 가능한 아이템 획득·귀가 없음·매행동 복원을 검사한다. 후속 검사는15개다.

## 여섯 번째 CI — 후속14/15 통과·위협 정보 공유

`c97f501`의 [CI38737692853](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38737692853)는 compile 성공, CTest12/13(19.08초), 후속14/15 통과다. 가시성·옛 예산 회귀는 통과했지만 seed21의 일일 종료는 남았다.

주인공 생존 경로는 저장된 visible 적 목록 전체의 공격 구역을 피하지만 동료는 자기2칸/방 시야만 사용해, 통로를 막는 알려진 적을 누구도 처리하지 않는 정보 불일치가 있다. 최신 적극적 동료 요구에 따라 동료는 같은 visible 적 목록을 공유하고 도달 가능한 가까운 적에게 접근한다. 적 AI의 기존 시야·추적은 유지한다. 전투 마무리 후 Return을 먼저 수행하고, 복귀 이동이 주인공 인접 칸에 도달하면 Follow로 전환해 다음 차례를 처리한다.

R15에 자기 시야 밖이지만 저장된 위협에 동료가 접근하는 조건을 추가했다. 후속은16개다. 알려진 적 목록 공유는 새 정책이며 원작 전체 AI 재현이라고 보고하지 않는다.

두 적의 기절 clip 길이 때문에 두 번째 주인공 공격이 왼쪽 적의 화면 제거보다 먼저 끝날 수 있다. GUI 검사는 두 개의 독립 P 공격 ID, 왼쪽 제거 뒤 오른쪽 Attack/Hurt/Faint 진행과 양쪽 제거 완료를 요구한다. 특정 공격이 기절 clip보다 늦게 시작해야 한다는 조건으로 합법적인 동시 진행을 실패시키지 않는다.


## 일곱 번째 CI — native/독립 GUI 통과, 기본 실행 종료 점검

`f37048f`의 [CI38739321236](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38739321236)는 native13/13(25.64초), 명시 후속15/16 통과다. 남은 후속 실패는4단계 직선 우선이다. WIC·playback·exploration·navigation·동료 양방향 독립 전투·entry·movement 등 선행 GUI 검사도 통과했다. 설치 패키지의 무인자 기본 실행을 닫을 때 비정상 종료가 있어 Windows 전체 검사는 아직 실패다. Linux/실행물 반영은 실행되지 않았다.

기본 실행 검사가 실패 시 종료 코드·events·snapshot·metrics를 보존하도록 보강하고, 새 빌드 직후 같은 무인자 실행을 먼저 재현한다. 오류 증거 없이 완료 판정을 바꾸지 않는다.


## 기본 실행 실패 축소

`2d4e829`의 [CI38741154753](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38741154753)에서4초 실행→닫기→같은 저장 재실행의 두 번째 실행만 종료 코드1로 실패했다. 저장 복원과10~13라운드의 동료/주인공 이동은 로그에 남았다. 최외곽 예외 처리에서 디버그 출력만 남기던 오류를 events.log에도 보존한다. 빠른 probe 전용 빌드에는 companion test 실행물이 없으므로 해당 진단은 실행물이 있을 때만 수행한다.


## 복원 후 이동 예외 원인과 수정

`2978861`의 [CI38741460793](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38741460793)에서 실제 오류는 `direction phase incompatible`였다. 독립 파티 renderer는 적 Animator를 Idle/Attack/Hurt에도 사용하지만, 종전 이동 연결부는 active이면 항상 Walk→Walk 방향 변경이라고 가정했다. 복원 뒤 대기 중이던 적의 첫 이동에서 Idle→Walk를 changeDirection으로 처리해 주기 길이 계약에 걸렸다.

독립 표시에서는 이동 보간만 등록하고 syncPartyActors가 실제 clip을 선택하도록 연결한다. Dialogue 등 종전 표시 구간으로 넘어간 독립 상태의 이동은 Walk를 새로 시작한다. 종전 단독 경로의 Walk 방향/위상 보존은 유지한다. 무인자 새 실행→저장 복원 probe와 기존 전체 GUI 검사를 다시 실행한다.
