# 동료 AI 후속 회귀 — 실행과 기준선

이번 검사는 [최신 요구](TEMP_HANDOFF_2026-10-15_PARTY_AI.md)의 실패를 고정하는 B 단계 회귀다. **런타임 수정 결과가 아니다.** 검사는 [tests/party_ai_regressions.hpp](../tests/party_ai_regressions.hpp)에 있고 기존 `pet_companion_tests`에서 명시적으로 실행한다.

## 실행

```sh
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --target pet_companion_tests
build/linux/pet_companion_tests --party-ai-followup
```

Windows MSVC에서는 `cmake --build build/windows --config Release --target pet_companion_tests` 뒤 `build/windows/Release/pet_companion_tests.exe --party-ai-followup`을 실행한다. 한 항목이라도 요구를 만족하지 않으면 종료코드1이다.

기존 기본 CTest와 GUI fixture/inspect 인자는 유지한다. 새 요구는 아직 미구현이므로 기본 CTest 성공과 이 명시 회귀 통과를 구분한다. 구현이 끝나면 통합 CI의 필수 검사로 연결한다. 실패를 정상 통과로 감추거나 예외를 제거하지 않는다.

## 실제 기준선

- 대상 런타임: `work=a4effe3`의 코드/자산/EXE. 마지막 런타임 소스는 `6ccb38c`이며 이번에는 바꾸지 않았다.
- 검증 커밋: `test/party-ai-regressions=f7da8de4242a1536b9000ae44bec5c5e74c9f3b7`.
- [CI38729302845](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38729302845)는 테스트 브랜치에서 같은 회귀를 기본 실행 끝에 호출해 알려진 실패를 출력한다. `work`에서는 위 명시 모드로 호출한다.
- [기계 판독 요약](validation/2026-10-15-party-ai-baseline.json), [Windows 발췌](validation/2026-10-15-party-ai-baseline-windows.txt), [Linux 발췌](validation/2026-10-15-party-ai-baseline-linux.txt)를 따른다.

| 항목 | 요구 검사 | 기준선 결과 |
| --- | --- | --- |
| R01 직선 | 주인공125→아이템129를 같은 행 직선으로 이동 | 실패.125→150→175→152→129 대각선 우회 |
| R02 HP시간 대조 | 적 없는 동일 fixture의 HP5/HP1 이동 시작/400ms 보간 | 통과. 양쪽1000/1400/1800/2200ms. 적 추적·전투·대기 때의 사용자 지연 원인을 해결한 결과는 아님 |
| R03 포위 | HP1, 적123–동료124–주인공125–적126. 적은 miss 고정 | 실패. 주인공 공격 전에 자동 귀가 |
| R03 막힘 | 이동 가능한 목표/행동이 없는 상태 | 실패. 대기 대신 귀가 |
| R04 양방향 | 같은 라운드에 주인공→적2, 동료→적1 | 실패. 주인공의 적2 공격만 기록되고 동료 공격 누락 |
| R05 한쪽 처치 | 왼쪽 적HP1 처치 뒤 오른쪽 주인공 공격 지속 | 통과. 이 단순 fixture에서 사용자 보고의 전체 정지 현상은 재현되지 않음 |
| R06 전투 유지 | 주인공HP1이어도 동료는 자신의 인접 적 공격 | 실패. 전역 도주 조건이 동료 공격 취소 |
| R06 떨어진 저장 | 주인공125·동료122의 합법 지형 전투 상태 저장 | 실패. `companion exceeded one-tile radius` |
| R07/R08 기존 완료·피해 | 잘못된 완료 주체 무시, 복원 뒤 대상 피해1회, 중복 완료 무해 | 통과. 기존 단일 조우의 소유권 대조이며 여러 적의 액션 ID 검증을 대신하지 않음 |
| R08 이동 복원 | 매행동 복원한 실행과 중단 없는 실행의 사건·RNG·상태 비교 | 실패.1400ms에 같은 목표129/round2에서 연속175, 복원126으로 다음 위치가 다름 |

## 추가로 드러난 저장·경로 문제

같은 저장 상태에서1400ms에 시작 위치150·동료150·목표129·round2가 같지만, 연속 실행은175로, 복원 실행은126으로 이동했다. 이 재현은 공격 피해 복원과 별개다.

현재 Explorer 복원 생성자는 행동권이 소비된 라운드의 playerOccupancy로 경로를 캐시한다. 이후 라운드가 바뀌어 동료가 다시 이동할 수 있어도 그 변화는 cachedWorld/cachedOccupancy에 포함되지 않는다. 이 경로를 우선 조사하고 라운드/배우 행동권과 경로 캐시 유효성의 관계를 수정해야 한다. 원인을 수정·검증하기 전 저장 복원 전체가 안전하다고 확대하지 않는다.

## 범위와 미검증

실제 사용자 HP1 지연·방 중복 snapshot, 복수 적 애니메이션의 동시 표시, 이전 MDES 진행 중 전투의 새 구조 이행,7층·관계별 색상은 후속 단계다. 실제 사용자 Windows11·잠금/절전·장기 성능 검사도 이번 기준선이 아니다. 테스트 fixture와 사용자 장면을 구분한다.
