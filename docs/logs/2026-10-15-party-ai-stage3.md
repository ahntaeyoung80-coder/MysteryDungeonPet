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
