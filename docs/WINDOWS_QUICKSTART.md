# Windows 실행

GitHub Desktop에서 `work`를 pull한 뒤 저장소 루트의 **MysteryDungeonPet.exe**를 더블클릭한다. ZIP 생성·다운로드·압축 해제·사용자 빌드는 필요 없다. EXE와 `assets/`를 저장소 안에 함께 둔다.

기본 실행은 기존 저장 복원 또는 자동 던전 탐험으로 시작한다. 저장과 로그는 `%LOCALAPPDATA%\MysteryDungeonPet`에 기록한다. pull로 기존 저장을 지우지 않는다. 다른 작업 폴더에서 EXE를 실행해도 자산은 EXE 위치를 기준으로 읽는다.

## 현재 동작

- 평상시 아이템 → 미인사 우호 NPC → 미탐색 영역/frontier → 발견된 계단. HP2 이하에서는 전투를 피하고 오랭열매/계단으로 도망간다. 계단을 먼저 발견했거나 열매가 없으면 계단을 우선한다.
- 오랭열매는HP3 이하에서만 사용한다. 열매 사용·아이템 획득은 각각1턴을 소비한다. 우호 NPC를 목표로 찾아가며, 길을 막는 우호 NPC와 처음 만나도 인사·응답 후 자리 교환으로 통과한다. 같은 NPC와의 인사는 반복하지 않는다.
- 계단 도착 →1초 fade-out → 다음 층 commit →1초 fade-in → 탐험 재개. 새 시작점과 이전 층 발밑 좌표에는 계단·아이템·배우를 배치하지 않는다. 한 탐험에서 같은 지형을 재사용하지 않으며 방 영역은 서로 겹치지 않는다.
- 노란 원반은 던전의 우호 NPC에게만 보인다. 플레이어·적·대화·생활 장면에는 없다.
- 공동 식사는7초다. 밤 침실에서 동료와 인사한 뒤 눕기·잠들기 animation을 한 번 재생하고 바로 fade-out/in으로 아침으로 전환한다. 수면 정지 장면과2초 대기는 생략한다.
- 동료와 함께 취침한다. 아침 fade-in 동안 두 배우의 잠든 마지막 자세 유지 → 두 배우 Wake14 → 두 배우 Nod22 인사 → 주인공 Walk0 왼쪽 통로 입구까지 약1.9초 이동 →750ms fade-out → 검정에서 다음날 전환 →750ms 탐험 fade-in. 동료 침상을 피해 맵 안에서 퇴실을 끝낸다.
- 피격 하트는 적 Attack1이 끝난 뒤 감소한다. 저체력 도주와 적 추적 이동은 같은400ms에 표시해 매 걸음 사이 별도 휴식이 없다.
- 새 저장은 MDES6이며 MDES1·2·3·4·5도 읽는다. 기술 선택·레벨·가방 등은 기존 범위 그대로다.

대각선 왕복은 동반 역주행 재계획으로 수정했으며, 같은 진행 상태가 반복되면 기존 다음 칸/직전 칸을 피하는 다른 방향으로 우회해 같은 층의 탐험을 계속한다. 같은 상태가 재등장하면 우회 후보를 순환하며 갈 수 있는 칸이 없으면 기다렸다가 다시 평가한다. 반복 감지는 귀가를 수행하지 않는다.

우클릭 메뉴는 Hide·Always on top·Scale1..3·**Reset 탐험 (귀가)**·Exit다. 탐험/전투/대화 중 Reset하면 확정 HP·획득품을 보존하고 귀가한 뒤 다음날 탐험으로 이어간다. 직전 상태는 저장 경로 뒤의 `.reset-backup`에 보존한다. 이미 귀가/생활 중이면 비활성화한다. [상세 원인·사용법·검증](NAVIGATION_RECOVERY.md)을 따른다. 원작 방향/animation 진단은 `MysteryDungeonPet.exe --diagnostics`로 실행한다. 새 실행 시작은 `--new-run`을 명시한다.

## 개발자 갱신과 검사

제품 수정 시 개발자가 EXE도 함께 갱신한다. Windows의 MSVC/CMake/Python 환경에서는 `powershell -ExecutionPolicy Bypass -File tools/update_windows_runtime.ps1`을 실행해 루트 EXE와 `windows-runtime.json`을 생성한다. Linux 교차 빌드는 `tools/check_windows_runtime.py --refresh-from <EXE> --compiler <설명>`으로 같은 기록을 생성한다. ZIP 단계는 없다.

`python tools/check_windows_runtime.py`는 EXE·소스·사용 자산 hash 일치를 검사한다. CI의 `tools/run_windows_checks.ps1`은 MSVC 빌드와 저장소 루트 EXE를 모두 다른 작업 폴더에서 기본 실행·저장 복원한다. 실제 Windows11 GUI·잠금/절전·장기 성능 확인 여부는 최신 검증 기록을 따른다.

이동·카메라 고정31시나리오와 `--movement-trace`의 프레임CSV/BMP는 [이동 검증 안내](MOVEMENT_VALIDATION.md)를 따른다. 최신 CI에서 중간 위치·방향 전환·1/2/3배 외곽·이동 중 강제 종료/복원과 기존 생활·전투가 통과했다.
