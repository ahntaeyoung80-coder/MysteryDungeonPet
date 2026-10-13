# B-1 탐험 종료·최소 전투 작업 현황

최신 길드 요구 보완: 주인공 방 취침·길드 식당12명 공동 식사를 연결했다. 식사는8초 유한 scene, Sleep은침상의EventSleep13이며 두 개체 Wake 완료를 기다린다. 현재 실행·검증은 [GUILD_LIFE_SCENE.md](GUILD_LIFE_SCENE.md)를 함께 따른다.

기준: 2026-10-13 사용자 최신 요구. 실제 제품 코드는 origin/work의3cbb95b에서 발견했다. 구현 위치는 `src/core/gameplay.cpp`, `src/core/exploration.cpp`, `src/core/state_store.cpp`, `src/windows/main.cpp`이며 별도 워크트리·브랜치는 PROJECT_STATE.md에 기록한다.

## 완료 항목

- [x] EXE 기본 실행을 자동 탐험으로 변경하고 진단 모드에는 `--diagnostics`를 추가. 탐험 메뉴에서 수동 동작·방향 제거.
- [ ] 설치된 EXE 무인자 실행·다른 작업 디렉터리·기존 저장 복원 Windows 재검증.

- [x] B-1 실제 브랜치·커밋·실행 진입점 확인 및 기존 코드 재사용.
- [x] 마지막 계단 → once Pose17 → ReturnHome, 목표·경로 캐시·탐험 이동 중단.
- [x] 750ms fade-out → 원작 집 배경 → 750ms fade-in. 실제 던전 귀가 이동 없음.
- [x] Eat15 소비 once → Sleep5 UTC deadline → 유한 Wake14 → Idle → 다음날 Explore.
- [x] 패배 귀가에 감정표현 cue 삽입; 승리는 Faint33 완료 뒤 같은 층·위치에서 재개.
- [x] 공격355 하나·플레이어 선공·3하트·적 내부 카운터2·동일 명중80%·치명타20%/5%·피해2배.
- [x] 관계3:1, 적대 인접 인식, 우호 대기·인접 인사.
- [x] 오랭열매70 최대1개·체력 부족 시 목표 우선·전투 중 텍스트만·2하트 회복·1턴 소비·적 턴.
- [x] MDES2 저장·복원, MDES1 호환, 중복 피해·소비·귀가 방지.
- [x] 아래12개 시나리오 각각 실행·기록. 기존 CTest/Python 회귀 검사와 Windows 크로스 빌드.

## 시나리오 결과

코어 실행은 `tests/gameplay_tests.cpp`, 각 시나리오 결과는 `docs/validation/2026-10-13-b1-gameplay-scenarios.jsonl`에 있다. 코어의 상태 검사와 Windows 실제 화면 실행을 구분한다.

| 번호 | 검사 | 코어 결과 |
| --- | --- | --- |
| 1 | 마지막 계단→Pose→귀가→식사→수면→기상→다음날 | pass; 실제 WAN17 Animator 완료와 생활 상태 연결 |
| 2 | 귀가 중 이동·재계획 없음 | pass; 위치/floorSteps/BFS 불변, target=-1, 귀가1500ms |
| 3 | 적대 조우·선공·공격·반격 | pass; 플레이어/적 모두 공격355 |
| 4 | 플레이어 일반 적중2회·Faint·탐험 재개 | pass; Faint 완료 전 승리 없음 |
| 5 | 플레이어 치명타 | pass; 카운터2 감소·피해2 |
| 6 | 적 일반 적중3회·사망·귀가·감정·식사·수면 | pass; 기상·다음날까지 연결 |
| 7 | 적 치명타 | pass; 체력2하트 감소 |
| 8 | 체력 부족 시 열매 우선 | pass; 가까운 일반 아이템보다 먼 열매 선택 |
| 9 | 전투 중 열매 사용 | pass; 텍스트·1→3하트·소비1회·적 턴 |
| 10 | 우호 조우 | pass; 인접 인사, 공격 없음, 탐험 유지 |
| 11 | 승리 후 귀가 없음 | pass; 원래 층·타일 유지 |
| 12 | transaction 복원 | pass; 모든 시나리오·20시드×2일 복원 비교, kill 이후 journal 불변 |

## 미완료·지원 한계

- [x] Windows CI38393535265 실제 실행 결과 수집: Server2025/MSVC 네이티브4CTests와 생활·전투 GUI 시나리오 pass. 귀가→식사1532.942ms, day2 Explore·전투 승리 후 Explore 확인.
- [ ] 실제 Windows11·잠금/절전·장기 성능 검증.
- [ ] 전체 MD별 적 몸체, 원작 계단·아이템 그림, 생활 소품·효과, 원작 시간·전체 자산 지원.
- [ ] 기존 이동 보간·카메라 스크롤의 사용자 체감 문제 전체 해결. 외곽 투명 노출 방지를 위해 카메라 clamp와 불투명 던전 바탕만 보완했다.

수면10초·기상 후 체력 회복·감정표현 Hurt6은 최소 신규 생활 정책이다. 원작 식사·침실 map과 몸체를 사용하지만 원작 전체 이벤트 VM은 구현하지 않는다. 요청에서 제외한 기술·레벨·가방·적 HP 표시·열매 사용 애니메이션·승리 자동 귀가는 추가하지 않았다.

## 실행

```sh
cmake -S . -B build/b1 -DCMAKE_BUILD_TYPE=Release
cmake --build build/b1
ctest --test-dir build/b1 --output-on-failure
build/b1/pet_gameplay_tests assets/original/gameplay
mkdir -p local
build/b1/pet_explore assets/original/exploration/dungeon-0/exploration.mdpe 42 local/day.mdes --gameplay
```

`pet_explore --gameplay`는 clip 완료를 공급하는 headless 시간 시뮬레이션이며 하루의 Ready까지만 실행한다. 같은 Ready 저장을 재실행하면 transaction을 반복하지 않는다. 실제 Windows 앱은 실시간 UTC·Animator 완료로 생활을 진행하고 Ready 이후 새날 탐험까지 이어간다. Windows는 설치물의 `MysteryDungeonPet.exe` 더블클릭으로 자동 실행한다. `Run-B1.ps1`도 호환 유지한다. 자동 Windows 검사는 `tests/gameplay_windows_tests.ps1`을 사용한다.
