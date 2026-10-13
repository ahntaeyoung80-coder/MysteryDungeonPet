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

현재 최소 구현은 기본공격355·3하트·오랭열매70 한 개이며, 기술 선택·레벨·가방은 없다. 길드원 원작 몸체와 식당·침실은 연결했고 별도 식사 효과·음악·원작 대사는 미완료다. 이동 보간·계단/아이템 원작 그림과 실제 Windows11 장기 검증도 남아 있다. 상세는 `B1_USAGE.md`, 저장소의 `docs/GUILD_LIFE_SCENE.md`를 따른다.
