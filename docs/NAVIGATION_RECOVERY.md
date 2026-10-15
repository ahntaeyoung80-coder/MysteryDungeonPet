# 대각선 왕복 수정과 Reset

## 원인과 수정

사용자가 보고한 3×3 기준 적2↔4, 플레이어6↔8 반복을 HP2에서 재현했다. 적이 직전 플레이어 위치로 추적한 뒤 플레이어가 매번 바뀐 공격 구역을 피해 재계획하면서 같은 두 배치로 돌아왔다. 각 이동은 성공하므로 기존 경로 실패 횟수로는 멈추지 않았다. 이전의 적 없는 고정 경로 검사는 이 결합을 다루지 못했다.

동반 역주행을 감지하면 그 한 행동에서 미래 공격 구역을 강제 제외하던 제약을 풀고 실제 점유를 지키는 목표 진행 경로를 계산한다. 적의 공격권과 충돌은 유지한다. 같은 joint-state가 최근32개 기록 안에 다시 나타나면 같은 층에서 합법 인접 칸으로 우회해 탐험을 계속한다. 가능하면 기존 다음 칸과 직전 칸을 피하고 적 공격 구역 수·미방문·목표 거리 순으로 후보를 정렬한다. 같은 상태가 재등장하면 후보 방향을 순환하며 저장/재실행 후에도 기록을 유지한다. 벽·대각선 모서리·점유·적 공격권을 지킨다. 출구가 하나면 그 칸을 사용하고 갈 수 있는 칸이 없으면 기다렸다가 다시 평가한다. 방문/발견/목표/소지/HP/추적 상태가 달라지면 별도 상태로 구분한다.

반복 감지로 귀가하거나 층을 바꾸지 않는다. 기존 목표로의 탐험을 이어가며 반복 대응으로 귀가는 사용자가 수동 Reset을 선택할 때만 수행한다. 이전 자동 귀가 정책은 사용자 정정으로 폐기했다. [단계별 명세](NAVIGATION_DETOUR_SPEC.md)를 따른다.

반복 기록은 MDES6에 보존하며 MDES1~5도 읽는다. 사용자의 기존 저장을 삭제할 필요가 없다.

## 수동 Reset

캐릭터 또는 트레이 아이콘 우클릭 → **Reset 탐험 (귀가)**. 탐험·전투·대화 중 사용할 수 있고 이미 귀가/생활 중이면 비활성화한다.

- 현재 탐험을 중단하고 기존 귀가 → 식사 → 취침/기상 → 다음날 탐험으로 이어간다.
- 확정 HP·획득 아이템·완료 기록을 보존한다. Reset 자체로 클리어/보상을 지급하지 않는다.
- 아직 피해가 반영되지 않은 공격과 남은 이동/추적을 취소한다.
- 현재 저장 경로 뒤에 `.reset-backup`을 붙인 파일로 직전 상태를 백업한다. 기본 실행은 `%LOCALAPPDATA%\MysteryDungeonPet\exploration.mdes.reset-backup`이다. 우회 이동은 일반 탐험 저장을 사용하며 수동 Reset 백업을 만들지 않는다.
- 중복 실행·Reset 직후 재실행에서도 귀가/식사를 한 번만 처리한다.

## 검증 재현

```sh
cmake -S . -B build/movement -DCMAKE_BUILD_TYPE=Release
cmake --build build/movement
ctest --test-dir build/movement -R pet_navigation_recovery_tests --output-on-failure
```

원래 배치, 저체력200배치의 매행동 저장/복원, 반복 기록/같은 층 우회·HP2/HP5 후보 순환·출구 없음의 대기/복원·유일 출구·모서리/공격권, 수동 Reset의 탐험/전투/대화·중복·다음날 연결, MDES5이전과 기존MDES1~4 호환 검사를 포함한다.

Windows MSVC/CMake/Python 환경에서는 `pwsh -File tools/run_windows_checks.ps1`을 실행한다. 새 navigation 검사는 원래 배치를 실제 앱에서 실행하고 반복 상태의 다른 방향 이동·같은 run/층 유지·재실행·우클릭 메뉴·Reset 명령·백업·중복 명령·재실행을 확인한다. 기존 전투/생활/기본 실행과 이동31시나리오도 함께 검사한다. 결과는 `build/windows/navigation-results`에 남는다. 실제 사용자 Windows11 화면 관찰과 구분한다.

## 최신 완료 결과 — 같은 층 우회

구현1babcf2의 [CI38692398718](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38692398718) core/windows 모두 성공했다. Linux ASan/UBSan10/10·Python26/26, Windows Server2025/MSVC19.51 native11/11, 실제 앱의 반복 감지 직후77→54 우회와 같은 run900/floor0 유지·재실행 후 자동 귀가0회를 확인했다. 수동 Reset의 우클릭 메뉴/백업/중복/재실행·식사1회, 기존 이동31시나리오·생활/전투·루트 EXE 기본 실행/복원도 통과했다.

[최신 검증 요약·hash](validation/2026-10-15-detour-summary.json), [반복 감지 첫 실행 CSV](validation/2026-10-15-detour-windows/watch-first-run.csv), [실행 events](validation/2026-10-15-detour-windows/events.txt), [저장 전후 상태](validation/2026-10-15-detour-windows/reset-states.json)를 보존했다. 상태 배열은 반복 감지 직전 → 우회 후 → 재실행 후 → 수동 Reset 직전 백업 → 수동 Reset 후 → 수동 Reset 재실행 후 순서다. 실제 사용자 Windows11 화면·OS 사건·장기 성능은 별도 미검증이다.

## 이전 자동 귀가 검증 — 2026-10-15

아래 결과는 사용자 정정 이전의 구현이며 최신 우회 정책의 검증 근거로 사용하지 않는다. 최신 결과는 PROJECT_STATE.md와 후속 detour 검증 요약을 따른다.

구현605c300의 [CI38674804212](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38674804212)는 core/windows 모두 성공했다. Linux ASan/UBSan10/10·Python26/26, Windows Server2025/MSVC19.51 native11/11, 실제 앱의 원래 왕복 탈출·지속 반복 자동 귀가·우클릭 메뉴/Reset·백업·중복 명령·재실행/식사1회가 통과했다. 기존 이동31시나리오·생활·전투·루트 EXE 기본 실행/복원도 통과했다.

[검증 요약·hash](validation/2026-10-15-navigation-summary.json), [메뉴](validation/2026-10-15-navigation-windows/reset-menu.txt), [Reset 전후 상태](validation/2026-10-15-navigation-windows/reset-states.json), [실행 events](validation/2026-10-15-navigation-windows/events.txt), [원래 배치 frame CSV](validation/2026-10-15-navigation-windows/cycle.csv)를 보존했다. Windows Reset 전후 상태 배열은 자동 복구 결과 → 수동 직전 백업 → 수동 Reset 결과 → 재실행 결과 순서다. 실제 사용자 Windows11 화면·OS 사건·장기 성능은 별도 미검증이다.
