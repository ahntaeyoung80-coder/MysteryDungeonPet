# 경로 진동 원인·재현

사용자 보고: 적2↔4, 플레이어6↔8 대각선 반복. 기준 work08a08ff, 미커밋 변경 없음.

HP2·플레이어77·적99·계단50의 열린 방에서 재현했다. 초기 이후(player102,enemy100)↔(player125,enemy77)를 반복한다. playerOccupancy가 적 주변 공격 가능 타일을 막고 scheduleDungeon/advance가 저체력 경로를 매번 재계획한다. 적은 이전 플레이어 위치에 접근하고 플레이어는 갱신된 위협 구역을 피하면서 같은두 상태로 복귀한다. 실패한 이동이 아니어서 targetFailures가 증가하지 않으며 selectSurvival에는 반복 상태를 끊는 gate가 없다. 기존 움직임 검사는 추적 적 없는 고정 경로였으므로 이 상호작용을 다루지 못했다.

새 회귀 tests/navigation_recovery_tests.cpp를 기존 코어에 연결하면24tick 내내 반복해 실패한다. 원본 로그는 docs/validation/2026-10-15-navigation-cycle-before.txt다. 다음은 동반 역주행 시 목표 진행 경로로 전환하고, 반복 상태를 저장해 자동 안전 귀가하는 watchdog과 수동 Reset을 구현한다. 수동 Reset은 현재 탐험 중단·귀가이며 inventory/완료 journal을 보존한다.
