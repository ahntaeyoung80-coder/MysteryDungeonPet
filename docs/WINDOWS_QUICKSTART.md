# Windows 실행

GitHub Desktop에서 `work`를 pull한 뒤 저장소 루트의 **MysteryDungeonPet.exe**를 더블클릭한다. ZIP 생성·다운로드·압축 해제·사용자 빌드는 필요 없다. EXE와 `assets/`를 저장소 안에 함께 둔다.

기본 실행은 기존 저장 복원 또는 자동 던전 탐험으로 시작한다. 저장과 로그는 `%LOCALAPPDATA%\MysteryDungeonPet`에 기록한다. pull로 기존 저장을 지우지 않는다. 다른 작업 폴더에서 EXE를 실행해도 자산은 EXE 위치를 기준으로 읽는다.

## 현재 동작

- 아이템 → 미탐색 영역/frontier → 발견된 계단. 진행 중인 목표는 완료/실패까지 유지한다.
- 계단 도착 →1초 fade-out → 다음 층 commit →1초 fade-in → 탐험 재개.
- 노란 원반은 던전의 우호 NPC에게만 보인다. 플레이어·적·대화·생활 장면에는 없다.
- 공동 식사는7초다. 이후 바로 밤 침실로 전환하고 기존 수면 animation·10초 마감을 유지한다.
- 수면 종료 →2초 대기 → 낮 배경/주인공 Wake14 → Walk0 방 이탈 → 다음날 탐험. 침실에는 주인공만 표시한다.
- 새 저장은 MDES4이며 MDES1·2·3도 읽는다. 기술 선택·레벨·가방 등은 기존 범위 그대로다.

우클릭 메뉴는 Hide·Always on top·Scale1..3·Exit다. 원작 방향/animation 진단은 `MysteryDungeonPet.exe --diagnostics`로 실행한다. 새 실행 시작은 `--new-run`을 명시한다.

## 개발자 갱신과 검사

제품 수정 시 개발자가 EXE도 함께 갱신한다. Windows의 MSVC/CMake/Python 환경에서는 `powershell -ExecutionPolicy Bypass -File tools/update_windows_runtime.ps1`을 실행해 루트 EXE와 `windows-runtime.json`을 생성한다. Linux 교차 빌드는 `tools/check_windows_runtime.py --refresh-from <EXE> --compiler <설명>`으로 같은 기록을 생성한다. ZIP 단계는 없다.

`python tools/check_windows_runtime.py`는 EXE·소스·사용 자산 hash 일치를 검사한다. CI의 `tools/run_windows_checks.ps1`은 MSVC 빌드와 저장소 루트 EXE를 모두 다른 작업 폴더에서 기본 실행·저장 복원한다. 실제 Windows11 GUI·잠금/절전·장기 성능 확인 여부는 최신 검증 기록을 따른다.
