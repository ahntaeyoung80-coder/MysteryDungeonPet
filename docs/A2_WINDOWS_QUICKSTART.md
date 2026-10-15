# Windows 11에서 A-2 실행

**2026-10-13 최신 재개 기준:** 사용자 실행 결과를 수집·점검했다. 사용자는 추가 수동 테스트를 진행하지 않기로 했으며, A-3는 기존 기록을 재사용해 시작한다. 아래 수동 확인 절차는 이전 빌드의 사용 안내이며 이번 작업의 재개 요구가 아니다. 현재 소스에는200px 고정·카메라 메뉴 제외와 계측 수정을 반영했다. 아래 다운로드 링크의 과거 A-2 EXE와 수정본을 구분하며 최신 [A-3 사용법](A3_USAGE.md)을 따른다. [점검·재개 기록](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/docs/logs/2026-10-13-a2-user-review.md)을 우선한다.

아래 과거 A-2 다운로드는 합성 시험 자산이다. 현재 소스의 A-3 설치물은 선택 원작runtime2개와 fixture를 포함하며 기본 EXE는 원작asset-0을 읽는다. 원본 ROM·전체 내부 파일은 설치하지 않고 원작 시간 검증은 별도다. Python·개발 서버·ROM 없이 실행된다.

현재 A-3 실행 ZIP은 [원작 패키지 Windows CI artifact](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38358343599/artifacts/11793603035)를 사용하며 [A-3 빠른 실행](A3_WINDOWS_QUICKSTART.md)을 따른다. 아래 과거 A-2 링크는 이력이다.

## 과거 A-2 다운로드

[Windows native 검사를 통과한 MSVC 실행 ZIP](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38339188747/artifacts/11786536952)을 다운로드한다. GitHub 로그인이 필요할 수 있다. 링크를 통해 받은 Actions ZIP은 압축 해제 후 **`package` 폴더 안**에서 실행한다. 함께 들어 있는 `wic-results`는 CI의 기존 검사 보고서다.

이 채팅에 제공된 실행 ZIP을 받았다면 `MysteryDungeonPet-A2` 폴더 안에서 실행한다. 두 패키지의 EXE는 같은 MSVC Release 빌드다. 채팅 ZIP에는 최신 안내 문서가 들어 있다.

이 빌드는 Windows Server2025의 CI에서 native core/WIC/투명도·캐시·오류 파일 검사를 통과했다. 사용자 Windows11의 창/포커스/잠금/전원/성능 검증은 아래 절차로 직접 수행한다.

## 바로 실행

1. ZIP 전체를 한 폴더에 해제한다. `MysteryDungeonPet.exe`, `assets/fixture/`, `licenses/`, `Run-A2.ps1`을 함께 둔다.
2. `MysteryDungeonPet.exe`를 더블클릭한다. 펫을 우클릭하거나 작업 표시줄의 숨겨진 아이콘에서 앱 아이콘을 우클릭하면 검사 메뉴가 열린다.
3. 몸체를 드래그한다. `Animation`, `Direction`, `Loop/Finite`, `Always on top`, `Test background`를 선택해 확인한다.
4. `Hide` 후에는 트레이 아이콘을 더블클릭해 복원한다. `Sleep`의 명시적 종료는 `Stop` 또는 다른 동작 선택이다. `Exit`로 종료한다.

현재 소스의 창은200×200 **실제 픽셀** 크기를 유지한다. 링크의 과거 A-2 빌드에는 이전 크기/카메라 메뉴가 남아 있다. DPI가 바뀌어도 정수 확대만 사용하며 장면은 창 내부에서 이동한다. 배경을 켜면 배경의 반투명 영역도 클릭 가능한 검사 영역이 된다.

## 결과를 남기며 실행

해제한 폴더를 PowerShell에서 연 뒤 실행한다. 관리자 권한은 필요 없다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run-A2.ps1
```

이 명령은 해당 프로세스의 스크립트 실행 정책만 설정한다. 시스템 실행 정책을 영구 변경하지 않는다. 실행이 조직 정책에 의해 제한되면 EXE를 직접 실행해도 기본 CSV가 남는다.

기본 WIC·해시·투명도·캐시 검사를 먼저 수행하고 창을 띄운다. 결과 위치는 출력된 `%LOCALAPPDATA%\MysteryDungeonPet\A2-날짜시간\`이다. EXE를 직접 실행하면 `%LOCALAPPDATA%\MysteryDungeonPet\metrics.csv`에 기록한다. 오류와 잠금·전원 사건은 같은 앱 폴더의 `events.log`에 기록한다.

자동 기준선 시나리오(약 9분)는 다음 명령이다. 각각 60초 warm-up 이후의 CPU·메모리를 비교할 수 있다. 창이 완전히 투명한 `empty`와 숨긴 `hide` 시나리오는 보이지 않는 것이 정상이다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run-A2.ps1 -Measure
```

빈 창·Idle·Walk·Hide 각 90초, 100회 동작/방향 전환 165초를 실행한다. CLI 크기 기본값은 200px/배율3이다. 원작·전 종 설치 크기나 최종 생활·탐험 성능의 결과로 확대하지 않는다.

## 참고용 수동 확인(이번 재개 요구 아님)

- 몸체 클릭·드래그가 되고 alpha0 영역은 아래 창의 클릭을 방해하지 않는지 확인한다. 창 표시·복원 시 다른 앱의 포커스를 뺏지 않는지 확인한다.
- 200px 고정, 항상 위 토글, 배경 경계, 8방향 및 1행 Eat/Sleep을 확인한다. 재실행하면 위치·크기·배율·항상 위 상태가 복원돼야 한다.
- 모니터 간 이동 또는 Windows 배율 변경 후 화면 밖으로 사라지지 않고 픽셀 경계가 흐려지지 않는지 확인한다.
- 실제 `Win+L` 후 해제, 디스플레이 끄기/켜기, 시작 메뉴의 절전 후 복귀를 수행한다. 중단 동안 렌더 제출이 증가하지 않고 복귀 후 프레임 폭주가 없어야 한다. 자동 복귀는 사용자 활동으로 분류하지 않는다.
- 앱 메뉴 `Exit`로 종료한 뒤 결과 폴더의 `*.csv`, `wic.txt`, `environment.json`과 `%LOCALAPPDATA%\MysteryDungeonPet\events.log`를 전달한다. 육안 확인 결과도 함께 알려준다.

여기서 연결된 Codex 실행기는 Linux이고 사용자 Windows PC의 명령 실행·GUI 조작 도구와 연결된 터미널이 없다. 사용자의 Windows 버전과 원격 실행기의 OS는 별개다. 사용자 PC의 OS 사건을 이 환경에서 대신 검증했다고 보고하지 않는다.

## 폴더를 여는 자세한 방법

파일 탐색기에서 ZIP을 우클릭해 「모두 압축 풀기」를 선택한다. 해제한 폴더 안에 EXE와 `assets`가 같은 수준으로 있는지 확인한다. EXE 하나만 다른 폴더로 옮기면 기본 패키지를 찾을 수 없다.

PowerShell을 여는 방법은 해제한 폴더의 탐색기 주소 표시줄에 `powershell`을 입력하고 Enter를 누르는 것이다. PowerShell 프롬프트의 현재 경로가 그 폴더인지 확인한 다음 위 명령을 붙여 넣는다. 공백·한글을 포함한 폴더 경로도 지원한다.

스크립트를 사용하지 않고 결과 파일 위치를 직접 지정하려면 다음처럼 실행한다. 경로를 따옴표로 감싸고 폴더는 먼저 만든다.

```powershell
New-Item -ItemType Directory -Force "$env:USERPROFILE\Desktop\A2-results" | Out-Null
.\MysteryDungeonPet.exe --metrics "$env:USERPROFILE\Desktop\A2-results\manual.csv"
```

다른 실행을 시작하기 전에 이전 앱을 트레이의 `Exit`로 종료한다. 자동 시나리오가 실행 중일 때 다른 앱 인스턴스를 동시에 실행하거나 설정을 바꾸면 시나리오 비교가 어려워진다.

## 메뉴별 검사 순서

| 순서 | 조작 | 정상적으로 기대할 결과 |
| --- | --- | --- |
| 1 | Animation → Walk, 방향 8개 선택 | 방향마다 시험 도형의 색/프레임이 바뀌며 기준점이 유지됨 |
| 2 | Idle 선택 | 같은 픽셀이 유지되는 동안 render submission이 계속 증가하지 않음 |
| 3 | Eat/Sleep에서 방향 변경 | 원본 계약이 1행인 시험 동작을 같은 행으로 재생함; 8방향 자산을 만든 것처럼 표시하지 않음 |
| 4 | Wake/Pose, Finite 선택 | 마지막 프레임에서 멈추고 finite_finish_count가 한 번 증가함 |
| 5 | Sleep, Loop → Stop | 계속 반복하다 명시적으로 정지함 |
| 6 | Scale1/2/3 | 창200px는 유지하고 도형 정수 확대가 변경됨 |
| 7 | Test background | 작은 검사 배경을 토글함; 현재 소스에는 카메라 조작 메뉴 없음 |
| 8 | Always on top 토글 | 다른 창과 겹칠 때 설정에 맞게 동작함 |
| 9 | 몸체 드래그 후 Exit·재실행 | 화면 안 위치, 크기·배율·topmost가 복원됨 |
| 10 | Hide·트레이 더블클릭 | 숨김 중 렌더가 중단되고 같은 장면으로 복원됨 |

현재 기본 자산은 파란/녹색 계열 합성 도형이다. 포켓몬 원작 그래픽을 배포한 패키지가 아니다. 이 도형으로 alpha·검정색·8방향/1행·시간/좌표 계약을 검증한다. 이전 A-1의 실제 원작 패키지 연결 명령은 저장소 `docs/A2_USAGE.md`에 있다.

## 실제 Windows 사건을 확인하는 방법

1. 수동 실행을 시작해 정상 재생을 확인한다. 잠금 테스트 전에는 20초 정도 기다려 정상 CSV 샘플을 남긴다.
2. `Win+L`로 실제 잠근 뒤 20초 이상 기다리고 로그인한다. 펫이 정상 복귀하는지 확인한다. 단순히 Hide를 실행한 결과는 잠금 검사로 기록하지 않는다.
3. 설정의 시스템 전원 메뉴에서 화면 끄기 시간을 짧게 설정할 수 있으면 실제 화면이 꺼질 때까지 입력하지 않고 기다린다. 검사 후 기존 시간 설정을 복원한다. 화면이 꺼진 후 마우스/키 입력으로 돌아왔을 때 재생을 확인한다. 가능하지 않으면 해당 항목을 `미실행`으로 기록한다.
4. 시작 메뉴 → 전원 → 절전으로 실제 절전한 뒤 20초 이상 기다리고 복귀한다. 복귀할 때 지나간 프레임이 빠르게 쏟아져 나오지 않아야 한다. PC가 절전을 지원하지 않으면 이유와 함께 `미실행`으로 남긴다.
5. 두 모니터가 있으면 배율이 다른 모니터로 드래그하고 화면 밖으로 사라지지 않는지 확인한다. 한 모니터만 있으면 디스플레이 배율 변경을 검사하거나, 환경을 바꾸기 어렵다면 DPI 항목을 미실행으로 남긴다.
6. `Exit`로 종료하고 출력 폴더의 마지막 CSV가 작성됐는지 확인한다. 잠금·전원 전환의 구체적인 시각은 `events.log`와 대조한다.

자동 `-Measure`가 끝났다고 실제 잠금·절전 검사를 통과한 것은 아니다. CSV에서 `render_paused=1` 구간의 제출 수를 대조하고, 본인이 실제 수행한 사건과 육안 결과를 함께 기록한다.

## 결과를 전달하는 방법

`Run-A2.ps1` 출력의 `Results:` 경로를 복사해 탐색기 주소 표시줄에 붙여 넣는다. `environment.json`, `wic.txt`, 실행한 `*.csv`를 포함한 폴더와 앱 폴더의 `events.log`를 ZIP으로 묶는다. 원작 ROM이나 원작 게임 자산은 결과 ZIP에 넣지 않는다.

아래 표를 채워 결과 ZIP과 함께 전달하면 Codex가 남은 판정을 갱신할 수 있다. 수행하지 못한 항목은 통과로 쓰지 않고 이유를 적는다.

| 검사 | 통과/실패/미실행 | 관찰·오류·미실행 사유 |
| --- | --- | --- |
| 실행/WIC report |  |  |
| alpha0 클릭 통과·몸체 드래그·포커스 |  |  |
| 200px 고정·topmost·저장/복원 |  |  |
| 동작/방향·Finite·Sleep Stop |  |  |
| 배경 clipping |  |  |
| 실제 DPI/모니터 변경 |  |  |
| 실제 Win+L·해제 |  |  |
| 실제 display off/on |  |  |
| 실제 절전·복귀 |  |  |
| 자동 기준선/100회 전환 |  |  |

## 실행이 안 될 때

- `cannot open ... fixture.mdpa`: 압축을 완전히 해제했는지, `assets/fixture/fixture.mdpa`가 EXE와 함께 있는지 확인한다.
- `atlas size/SHA-256` 또는 WIC 오류: 원래 ZIP을 새 폴더에 다시 해제하고 결과 report를 전달한다. 다른 PNG로 바꿔 검사를 통과시키지 않는다.
- 창이 보이지 않음: 트레이 아이콘 더블클릭으로 복원한다. `empty`/`hide` 자동 시나리오는 표시가 없는 상태를 검사한다. 수동 실행에서도 보이지 않으면 `events.log`의 초기 세션 상태/OS 오류를 전달한다.
- PowerShell에서 스크립트를 찾지 못함: 현재 경로가 해제 폴더인지 확인하고 `Get-ChildItem`으로 `Run-A2.ps1`을 확인한다.
- 저장 위치가 이상함: 앱을 종료한 뒤 `%LOCALAPPDATA%\MysteryDungeonPet\window.state`를 다른 이름으로 옮겨 기본 위치로 시작할 수 있다. 기존 파일은 복원할 수 있게 보관한다.
- 오류 상자가 열림: 오류 전체 문구와 `events.log`를 전달한다. HRESULT/OS 숫자는 원인 분석에 필요하다.
