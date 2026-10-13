# A-3 통합 검증 착수

2026-10-13 KST. `origin/work`의 A-1/A-2 구현과 기존 Windows11 사용자 기록을 이어받았다. [재개 지침](logs/2026-10-13-a2-user-review.md)의 계측·창 요구를 보완하고 첨부 원작 package의 native 연결을 검증했다. 실제 Windows 원작 통합과 A 전체 완료는 pending이다. 추가 사용자 수동 테스트를 요청하지 않는다.

## 현재 변경

- 창은 200×200 실제px 고정이다. 크기 선택·카메라 조작 메뉴를 제거했다. `--size 200`은 이전 호출과의 호환을 위해 허용하며 다른 값은 거부한다. 기존 버전1 저장 파일의 100px 값은 읽되 위치·배율·topmost만 복원한다.
- CSV에 동작ID·요청 방향·실제 방향행·프레임·Animator 활성·장면 좌표·tick·rawFlag·anchor·atlas rect를 추가했다. Renderer와 CSV는 같은 `PlaybackSample`의 프레임과 좌표를 사용한다. 동작/방향 선택 사건은 `events.log`에도 기록한다.
- `final_sample=1`인 마지막 행은 창 파괴 전에 한 번 기록한다. 실제 HWND의 크기와 DPI를 확인한다. 파괴 후 DPI0/위치0,0 행을 추가하지 않는다.
- `empty`는 Animator를 시작하지 않고 프레임 deadline도 예약하지 않는다. 프레임/동작/방향은 -1, 활성·marker·완료·skipped cycle은 0이다. 최초 atlas decode는 기존처럼 수행하므로 최초 자산 로드까지 제외한 기준선은 아니다.
- 렌더 시 `advance()`가 반환한 다음 deadline을 저장해 예약에 재사용한다. 예약 경로에서 다시 `advance()`를 호출해 사건을 소비하지 않는다.

CSV는 각 샘플 시점의 선택된 재생 상태다. 중단 중 동작을 변경하면 선택된 첫 프레임이 기록되며, 그것이 마지막으로 제출한 픽셀이라는 의미는 아니다. 1초 샘플로 모든 프레임의 실제 표시 시간이나 원작 wall time을 판정하지 않는다. `Sleep`의 명시적 Stop은 몸체를 유지하고 활성만 0으로 바뀐다.

## 자동 검사

기존 A-1/A-2 검사와 native 코어 검사는 재사용한다.

```bash
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure
PET_PACKAGE_INSPECT=$PWD/build/linux/pet_package_inspect .venv/bin/python -m unittest discover -s tests -v
```

새 CSV를 실제 native package 해석과 대조하는 명령:

```bash
# 아래 CSV는 실제 실행에서 생성돼야 한다. 과거 A-2 CSV는 새 필드가 없으므로 거부한다.
.venv/bin/python tools/check_playback_metrics.py local/results/walk.csv \
  --package assets/fixture/fixture.mdpa --inspector build/linux/pet_package_inspect \
  --scenario walk --output local/results/walk-check.json
```

검사는 frame/tick/flag/anchor/rect·1/8행 resolver·고정 장면 좌표·유효 DPI·마지막 행·empty 오염 여부를 확인한다. CLI 입력과 같은 asset을 기록했는지 실행 절차에서 보존해야 한다. 검사 보고서에는 CSV/MDPA 해시와 sourceKind를 남긴다. 로그 자체를 위조하지 않았다는 증명이나 원작 시간·8방향 전체 재생 완료 판정은 아니다.

`tools/run_windows_checks.ps1`과 GitHub Actions는 기존 MSVC Release/core/WIC 검사 후 `tests/playback_metrics_tests.ps1`을 실행한다. fixture 및 원작2개 각각의 empty/Walk 방향5/Idle 또는alias 방향4/Eat 방향7/Wake 방향2를3초씩 실행해 로그를 수집하고 native inspector와 대조한다. CI의 세션이 잠겨 있으면 프레임 진전 없이도 메타데이터 검사는 가능하다. 이 결과를 실제 사용자 Win+L·display-off·절전·전 방향 통과로 확대하지 않는다. 원작2개의 native WIC/hash/cache와 모든 clip/방향의 native tick 재생도 검사한다. CI CSV/JSON과 선택 설치물은 호환 artifact명 `a2-windows-fixture`에 보존한다.

## 다음 실제 연결

사용자가 이전 세션의 `a1-original-package.zip`을 첨부했다. 원작 선택 package를 `local/a3-input/a1-rom-final-01`에 복원했고 변환·native 대조를 수행했다. 이후 원격 `work`의4231a0c가 사용자 명시 지시에 따라 원본 입력을 assets/original/에 보관했다. 현재 ROM과 공유 runtime도 사용할 수 있다. 아래는 이번 수행 명령이며 출력 폴더를 재사용해 덮어쓰지 않는다.

```bash
.venv/bin/python tools/compile_runtime_package.py --package local/a3-input/a1-rom-final-01 --out local/a3-rom-runtime-01
```

추가 native 검증 명령:

```bash
.venv/bin/python tools/verify_a3_package.py --package local/a3-input/a1-rom-final-01 \
  --runtime local/a3-rom-runtime-01 --inspector build/linux/pet_package_inspect \
  --playback build/linux/pet_package_playback --report local/a3-original-integration.json
```

대표2개 자산/348프레임의 RGBA 재조립과 A-1→MDPA→native 메타데이터 대조, 8clip×8방향×2개=128선택의 tick 경계·finite·marker once·pause/resume·지연·Stop을 통과했다. 1행 Eat/Sleep은 같은 행을8개 요청 방향으로 검사하므로8방향 원작 시트를 만들었다는 뜻은 아니다. 패키지2회 변환 결과가 동일하며 첨부219파일을 ZIP 원문과 바이트 대조했다. `verify_a3_package.py`는 WAN ID가 있는 대표 패키지를 대상으로 하며 알 수 없는 selector를 임의 생성하지 않는다. 원작 시간이 미확정인 잠정60Hz는 유지한다.

현재 Windows 코드/fixture CI와 원작 패키지의 Linux native 결과는 별개다. 실제 원작 패키지·Windows·원작 시간 자료를 연결한 뒤 pipeline §7의 통합 항목을 판정한다. 공유 ROM의 대표 재추출은 동일 manifest와 원본 불변을 확인했다. 원작 anchor/shadow의 실제 표시·wall time·기록에 없는 OS 사건·최종 성능은 pending이다. B/C는 시작하지 않는다.

## Windows 선택 설치물

기본 EXE는 `assets/original/asset-0/asset.mdpa`를 읽는다. `--package`로 asset-1 또는 시험 fixture를 선택할 수 있다. 빌드/설치에는 원작 선택runtime2개와 계약 검사용fixture·크레딧/라이선스를 포함하며 원본 ROM·전체 파일 ZIP·inventory는 포함하지 않는다. 과거 A-2 artifact의 EXE와 이번 수정본을 구분한다.

```powershell
.\MysteryDungeonPet.exe --package assets\original\asset-1\asset.mdpa --clip 17 --direction 0 --finite
```

위는 사용 예이며 추가 사용자 수동 테스트 요청이 아니다.
