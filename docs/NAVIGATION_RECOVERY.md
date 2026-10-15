# 대각선 왕복 수정과 Reset

## 원인과 수정

사용자가 보고한 3×3 기준 적2↔4, 플레이어6↔8 반복을 HP2에서 재현했다. 적이 직전 플레이어 위치로 추적한 뒤 플레이어가 매번 바뀐 공격 구역을 피해 재계획하면서 같은 두 배치로 돌아왔다. 각 이동은 성공하므로 기존 경로 실패 횟수로는 멈추지 않았다. 이전의 적 없는 고정 경로 검사는 이 결합을 다루지 못했다.

동반 역주행을 감지하면 그 한 행동에서 미래 공격 구역을 강제 제외하던 제약을 풀고 실제 점유를 지키는 목표 진행 경로를 계산한다. 적의 공격권과 충돌은 유지한다. 같은 joint-state가 최근32개 기록 안에 다시 나타나면 자동으로 탐험을 중단하고 귀가한다. 방문/발견/목표/소지/HP/추적 상태가 달라지면 별도 상태로 구분한다.

반복 기록은 MDES6에 보존하며 MDES1~5도 읽는다. 사용자의 기존 저장을 삭제할 필요가 없다.

## 수동 Reset

캐릭터 또는 트레이 아이콘 우클릭 → **Reset 탐험 (귀가)**. 탐험·전투·대화 중 사용할 수 있고 이미 귀가/생활 중이면 비활성화한다.

- 현재 탐험을 중단하고 기존 귀가 → 식사 → 취침/기상 → 다음날 탐험으로 이어간다.
- 확정 HP·획득 아이템·완료 기록을 보존한다. Reset 자체로 클리어/보상을 지급하지 않는다.
- 아직 피해가 반영되지 않은 공격과 남은 이동/추적을 취소한다.
- 현재 저장 경로 뒤에 `.reset-backup`을 붙인 파일로 직전 상태를 백업한다. 기본 실행은 `%LOCALAPPDATA%\MysteryDungeonPet\exploration.mdes.reset-backup`이다. 자동 복구도 같은 방식으로 직전 상태를 보존한다.
- 중복 실행·Reset 직후 재실행에서도 귀가/식사를 한 번만 처리한다.

## 검증 재현

```sh
cmake -S . -B build/movement -DCMAKE_BUILD_TYPE=Release
cmake --build build/movement
ctest --test-dir build/movement -R pet_navigation_recovery_tests --output-on-failure
```

원래 배치, 저체력200배치의 매행동 저장/복원, 지속되는 반복 기록/자동 귀가, 수동 Reset의 탐험/전투/대화·중복·다음날 연결, MDES5이전과 기존MDES1~4 호환 검사를 포함한다.

Windows MSVC/CMake/Python 환경에서는 `pwsh -File tools/run_windows_checks.ps1`을 실행한다. 새 navigation 검사는 원래 배치를 실제 앱에서 실행하고 자동 복구·우클릭 메뉴·Reset 명령·백업·중복 명령·재실행을 확인한다. 기존 전투/생활/기본 실행과 이동31시나리오도 함께 검사한다. 결과는 `build/windows/navigation-results`에 남는다. 실제 사용자 Windows11 화면 관찰과 구분한다.

## 완료 결과 — 2026-10-15

구현605c300의 [CI38674804212](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38674804212)는 core/windows 모두 성공했다. Linux ASan/UBSan10/10·Python26/26, Windows Server2025/MSVC19.51 native11/11, 실제 앱의 원래 왕복 탈출·지속 반복 자동 귀가·우클릭 메뉴/Reset·백업·중복 명령·재실행/식사1회가 통과했다. 기존 이동31시나리오·생활·전투·루트 EXE 기본 실행/복원도 통과했다.

[검증 요약·hash](validation/2026-10-15-navigation-summary.json), [메뉴](validation/2026-10-15-navigation-windows/reset-menu.txt), [Reset 전후 상태](validation/2026-10-15-navigation-windows/reset-states.json), [실행 events](validation/2026-10-15-navigation-windows/events.txt), [원래 배치 frame CSV](validation/2026-10-15-navigation-windows/cycle.csv)를 보존했다. Windows Reset 전후 상태 배열은 자동 복구 결과 → 수동 직전 백업 → 수동 Reset 결과 → 재실행 결과 순서다. 실제 사용자 Windows11 화면·OS 사건·장기 성능은 별도 미검증이다.
