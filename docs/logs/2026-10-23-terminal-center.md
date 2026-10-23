# 마지막 맵 중앙 클리어 긴급 수정

사용자 지시: 마지막 맵에서 목적 없이 배회하지 않고 중앙으로 걸어가서 클리어 후 귀가한다.

## 변경

- 최종 원정층의 이동 목표를 일반 탐색·계단에서 맵 중앙으로 분리했다. 실제 보행 영역 경계의 중앙에 가장 가까운, 입구와 연결된 보행 타일을 선택한다. 원본 지형·출구 좌표는 보존한다.
- 남은 고정 보스 전투는 기존 규칙으로 처리한다. 전투가 끝나면 아이템·친구·미탐색 구역보다 중앙 이동을 우선한다. 중앙 도착은 전체 방 탐색 여부와 무관하게 기존 클리어·성공 집계·귀가·식사로 연결된다. 일반 층과 중간 구간의 출구는 그대로다.
- 단독·독립 파티 모두 같은 도착 지점을 사용한다. 이전 출구 목표와 완료 위치도 저장 검증에서 허용하여 기존 저장을 읽는다. 최종층에서 예전 목표를 다음 행동 때 중앙으로 다시 잡는다. 저장 스키마 변경은 없다.

## 검증

- 기존 `pet_expedition_tests`의 쉼터·고정 보스 건너뛰기 방지·성공·귀가·복원 검사 통과.
- 새 `pet_expedition_tests --terminal-walk /workspace/MysteryDungeonPet`: 8개 원정 각각 단독/동료, 총 16경로 통과. 이전 층은 전환 fixture로 진행하고 마지막 층은 입구부터 실제 AI 이동한다. 매 갱신마다 저장 복원 결과를 비교한다. 이전 출구 목표·실패 3회가 남은 저장에서도 중앙 도착·성공·귀가 1회·식사 1회를 확인한다. 보스는 검사용으로 처치 상태를 만들며 자연 전투 승리 전체를 검증한 것은 아니다.
- 새 Windows x64 static Release의 `pet_expedition_tests.exe`를 Wine 10에서 실행해 통과.
- 실제 새 EXE로 원정 14 최종층을 35초 실행(프로세스 실측 36.53초, 정상 종료 0). 중앙 도착 → CompletionFadeIn/Walk/Pose/Text/FadeOut → ReturnHome → Eat/Sleep/Wake/Ready → 다음 날 입장 확인. 마지막 저장 runId 9301, day 2, meals 1. [결과](../validation/2026-10-23-terminal-center/summary.json), [성공 화면](../validation/2026-10-23-terminal-center/completion.png).
- Wine/Xvfb와 합성 WTS 해제를 사용한 검사다. 실제 Windows 11 검증으로 표시하지 않는다.
- 루트 EXE 갱신 후 `python tools/check_windows_runtime.py` 통과. EXE SHA-256: `9256ee0efd7b6d7e916ca1c53cbf8d2f35b993b53c3082e356447783d470492e`.

재현: `cmake --build build/playtest-asan --target pet_expedition_tests`; `build/playtest-asan/pet_expedition_tests`; `build/playtest-asan/pet_expedition_tests --terminal-walk /workspace/MysteryDungeonPet`.
GUI 입구 저장 생성: 마지막 명령에 출력 디렉터리를 추가한다. `terminal-walk-14.mdes`를 `--snapshot`에, 원정 14의 `exploration.mdpe`를 `--explore`에 지정해 새 EXE로 실행한다.

## 시간·검사 선택

착수 시 구현 10~20분, 관련 검사·EXE 반영 10~15분의 범위 추정을 보고했다. 최초 조사 시간은 별도로 계측하지 않았다. 명시 시각 계측은 03:47:48 UTC부터 시작했고 구현·크로스 빌드·관련 검사·문서화를 병행했다. 헤더 의존 빌드 도중 같은 native 빌드를 중복 시작한 구간이 있어 개별 컴파일 시간은 분리하지 않는다. 두 빌드가 종료된 뒤 순차 빌드에서 `no work to do`를 확인하고 검사를 실행했다.

이번 변경은 최종층 동선·판정에 한정한다. 기존 ASan 빌드를 재사용해 관련 원정 검사만 실행했으며 전체 메모리/32개 코어/Python 검사를 다시 실행하지 않았다. 코드 push는 전체 CI를 시작하므로 제품 반영 push는 한 번만 하고 CI는 백그라운드로 둔다. 원격 CI 결과는 아직 미확인이다.

명시 계측 구간 종료: 03:54:27 UTC, 경과 400초(구현·검사·문서 병행, 초기 조사·후속 원격 전송 제외).
