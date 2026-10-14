# Windows 자동 탐험 실행

1. ZIP 전체를 새 폴더에 해제한다. EXE 옆의 `assets` 폴더도 함께 필요하다.
2. `MysteryDungeonPet.exe`를 더블클릭한다. 별도 인자나 PowerShell 없이 dungeon0 자동 탐험을 시작한다.
3. 적대 조우에서는 기본공격 전투를 진행한다. 승리는 같은 던전에서 탐험을 계속하고, 패배나 탐험 종료는 길드로 귀가한다.
4. 길드 식당의 공동 식사 → 주인공 방 취침 → 주인공·동료 기상 → 다음날 탐험을 자동으로 이어간다.

우클릭 메뉴에는 Hide/Show, Always on top, 배율, Exit가 있다. 방향·동작을 직접 선택하는 메뉴는 진단 모드에만 표시한다. 창은200×200 실제px다. Hide 후 트레이 아이콘을 더블클릭하면 다시 표시한다.

종료 후 재실행하면 `%LOCALAPPDATA%\MysteryDungeonPet\exploration.mdes`에서 탐험·전투·생활 상태를 복원한다. 저장 상태에 따라 길드 장면부터 이어질 수 있다. `Run-B1.ps1`도 계속 사용할 수 있으며, 새 탐험을 명시적으로 시작하려면 `Run-B1.ps1 -NewRun` 또는 아래 명령을 사용한다. `--new-run`은 현재 저장 위치의 이전 진행을 새 run으로 교체한다.

```powershell
.\MysteryDungeonPet.exe --new-run --seed 42
```

개발용 방향·애니메이션 검사는 다음처럼 명시한다.

```powershell
.\MysteryDungeonPet.exe --diagnostics
```

기존 `--package`, `--clip`, `--direction`, `--scenario`, `--finite`, `--validate-package` 명령도 A 진단 모드로 동작한다. 명시적인 `--explore`는 기존 탐험 실행 방식으로 유지한다. `--metrics`, `--duration`, `--seed`, `--snapshot`만 지정하면 자동 탐험을 유지한다.

현재 구현은 8방향 자동 이동·계단 우선 탐험·적 추적·기본공격355·5하트·오랭열매70 한 개다. 승리 후 탐험을 재개하고, 사망하면 Faint 뒤 검정 화면을 거쳐 귀가한다. 우호 NPC는 서로 마주보며 한국어 대화를 나눈 뒤 탐험을 이어간다.

길드에서는 전체 식당 배경의 카메라 이동·공동 식사, 낮 침실의 Nod/Laying, 밤 배경의 취침, 두 배우 기상·Nod, 다음날 탐험을 연결했다. 원본 열매·계단과 우호 배우의 노란 원반을 사용한다. 식당 이동4초·식사8초·수면10초 등의 장면 시간은 최소 구현의 선택이며 원작 하루 전체 재현은 아니다.

MDES1·2 읽기를 지원하며 새 저장은 MDES3이다. 과거 완료 NPC는 원래 제거된 상태로 복원하고, 새 대화가 완료된 NPC는 계속 표시한다. 기술 선택·레벨·가방·원작 대사 복원·식사 부가 효과·음악은 이번 범위에 포함하지 않는다. 실제 Windows11 화면·잠금/절전·장기 성능 검증은 남아 있다. 상세는 저장소의 `docs/IMPROVEMENT_HANDOFF.md`, `docs/implementation/MDES3_FORMAT.md`를 따른다.
