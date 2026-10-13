# A-2 구현·빌드·검증

최신 사용자 점검과 재개 판정은 [A-2 사용자 실행 기록 점검](logs/2026-10-13-a2-user-review.md)을 따른다. 추가 수동 테스트 없이 기존 Windows11 측정을 재사용해 A-3에 진입하고, 로그/기준선 오류와200px 고정·카메라 메뉴 제거는 A-3 시작 작업으로 처리한다. 이번 저장에서 제품 코드는 수정하지 않았다.

C++17/Win32/WIC 기반 시험 런타임이다. A-1의 대표 원작 준비 완료 판정은 유지하며, A-2의 실제 Windows UI·전원·성능 완료 판정은 대기 상태다. 최신 원격 A-1 구현 `4d6ed6b`를 받아 기존 Extractor/Packer를 재사용했다.

## 실행 범위

- `src/core/`: MDPA 패키지 검증, alias 참조/개별 WAN 메타데이터, 정수 tick·monotonic deadline Animator, 카메라·nearest·PBGRA 합성.
- `src/windows/`: SHA-256/바이트/크기 검증 후 WIC 32bppPBGRA 디코드·단일 atlas 캐시, top-down DIB/UpdateLayeredWindow, 투명 hit test·드래그·topmost·100/200 실제 px·PMv2 DPI·화면 내 위치, 트레이와 검사 메뉴.
- 숨김/세션 잠금·연결 해제/디스플레이 off/절전 때 렌더 중단. 초기 WTS 잠금 상태 조회와 최대3회 알림 등록 재시도. 절전 복귀는 디스플레이 on 알림과 잠금 해제 조건을 따른다. 자동 복귀를 사용자 활동으로 분류하는 코드가 없다.
- 1 UI 스레드, one-shot waitable timer와 메시지 대기. 캐시 miss만 디코드하고 동일 합성 픽셀은 재제출하지 않는다. PNG 재디코드·BFS·게임 엔진·Python 상주 없음.
- `%LOCALAPPDATA%/MysteryDungeonPet/window.state`: 버전1, 창 위치·크기·배율·topmost. 임시 파일/flush/교체, 손상 파일 거부. 명시 CLI 크기/배율이 저장값보다 우선한다.
- CSV: 한 코어/전체 코어 CPU, working set/private/peak, 제출/디코드/marker/finite finish/건너뛴 loop cycle, handle/GDI 수, 중단 상태, DPI·위치·전환 수. 사건/HRESULT/OS 오류는 `events.log`.

그림자 mask·size·원본 offset·marker는 패키지에 보존한다. 현재 표시기는 몸체와 작은 검사 배경을 합성한다. SkyTemple의 RGB 크기 mask와 흰 원점 픽셀을 실제 그림자로 그리지 않는다. 원작 그림자 표시·wall time 정확성은 A-3에서 별도 확인한다.

## Windows 사용자가 실행할 최소 절차

완성 ZIP 사용은 [Windows 빠른 실행](A2_WINDOWS_QUICKSTART.md)을 따른다. Python/MSVC 설치 없이 EXE를 실행할 수 있다. `Run-A2.ps1`은 native WIC 검사 후 창을 실행하고 결과 폴더를 출력한다.

개발 빌드는 Visual Studio 2022의 「Desktop development with C++」, Windows SDK, CMake, Python3가 필요하다. 이 Python은 fixture 검사/결과 수집만 사용한다.

```powershell
# 저장소 루트 PowerShell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\run_windows_checks.ps1
# 실제 데스크톱의 자동 기준선과 100회 전환도 실행 (약 9분)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\run_windows_checks.ps1 -Interactive
```

직접 빌드/실행:

```powershell
cmake -S . -B build/windows -A x64
cmake --build build/windows --config Release
ctest --test-dir build/windows -C Release --output-on-failure
.\build\windows\Release\MysteryDungeonPet.exe
cmake --install build/windows --config Release --prefix build/windows/package
```

MSVC는 `/MT` Release, 내장 PMv2 manifest, User32/Gdi32/Ole32/WIC/Wtsapi32/Shell32/Uuid/Psapi/Bcrypt를 사용한다. Windows10 1703 이상/11 기준이다. Windows 시스템 DLL을 설치물에 복사하지 않는다.

## A-1 패키지 연결

A-1 `manifest.json`이 의미 계약의 원본이다. 같은 계약을 런타임용 MDPA로 직렬화하고 PNG는 변경 없이 복사한다. runtime은 JSON을 파싱하지 않는다. 기존 ROM 성공 추출을 반복할 필요가 없다.

```bash
# 고정 A-1 환경. 기존 package를 읽기 전용으로 사용, 새 출력 디렉터리 필수.
.venv/bin/python tools/compile_runtime_package.py --package local/a1-rom-final-01 --out local/a2-rom-runtime-01
# 현재 환경처럼 기존 local/ 자산이 없는 경우 합성 fixture 연결만 확인.
.venv/bin/python -m tools.mdpet fixture --out local/a2-a1-fixture
.venv/bin/python tools/compile_runtime_package.py --package local/a2-a1-fixture --out local/a2-a1-runtime
```

각 asset은 `asset-N/asset.mdpa`, `atlas.png`, `CREDITS.txt`로 출력된다. index는 오프라인 안내이며 runtime 입력은 개별 MDPA다. 단일 atlas decoded/PNG 예산은 각각8MiB, manifest2MiB다. 초과 시 명시 오류로 끝내고 선택/분할을 요구한다. 한 번에 한 asset을 캐시하며 reload 때 같은 hash/dimension의 atlas는 공유한다. cache miss의 기존+후보 atlas 임시 peak는 정상 resident 캐시 예산과 구분한다.

```powershell
.\MysteryDungeonPet.exe --package C:\path\a2-rom-runtime-01\asset-0\asset.mdpa --clip 17 --direction 0 --finite
```

`--clip`, `--direction 0..7`, `--finite`, `--size 100|200`, `--scale 1..8`, `--metrics 경로`, `--duration 초`, `--scenario manual|empty|idle|walk|hide|switch`, `--validate-package`, `--report 경로`를 지원한다. CLI size/scale을 지정하지 않은 manual 실행에서는 저장값을 사용한다. Sleep loop는 Stop/다른 clip 선택으로 종료한다.

A-1 alias의 `rawDirections`는 대상 atlas를 공유하되 자기 tick/flag/marker/프레임 수를 가진다. 원본 geometry가 다른 alias는 임의 이동·대체하지 않고 변환을 거부한다. 원본 tickHz 미확정이면 A-1의 provisional60Hz를 그대로 보존하며 원작 시간 검증 성공으로 표기하지 않는다. rawSpeed 의미가 지정되지 않은 값은 임의 적용하지 않고 거부한다.

## Linux 검증과 크로스 빌드

```bash
python3 tools/make_fixture.py --check
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure
PET_PACKAGE_INSPECT=$PWD/build/linux/pet_package_inspect .venv/bin/python -m unittest discover -s tests -v
cmake -S . -B build/sanitized -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build/sanitized
ctest --test-dir build/sanitized --output-on-failure
# 설치된 MinGW-w64 toolchain 사용
cmake -S . -B build/mingw -DCMAKE_TOOLCHAIN_FILE=cmake/mingw.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/mingw
cmake --install build/mingw --prefix build/package-mingw
```

현재 cloud 환경의 toolchain은 저장소 밖 `/workspace/toolchain`에 Debian13 패키지를 SHA-256 확인 후 사용자 권한으로 해제했다. cmake4.4.4/ninja1.13.2, GNU14.2/MinGW14-posix 사용. 해당 설치 경로는 제품 의존성이 아니다. Linux 크로스 빌드는 Windows 실행 증거가 아니다.

`.github/workflows/a2.yml`은 Linux sanitizer/A-1 연결과 Windows MSVC Release/core/native WIC/8개 오류 패키지를 검사한다. CI의 headless WIC 통과와 실제 데스크톱/잠금/절전 검증을 구분한다. 실제 Windows Server2025/MSVC19.51 Release/core/WIC/8개 오류 검사 pass(run38339188747). 결과는 로그와 `docs/validation/2026-10-13-a2-windows-ci.txt`에 기록했다. 이후 제출된 사용자 Windows11 성능·Hide·전환 기록은 별도 사용자 점검 JSON에 보존했다. 명시 증거가 없는 OS 사건은 미검증이다.

## 완료 판정과 재개

독립 로직·소비 계약·크로스 빌드 통과는 부분 결과다. 실제 Release Windows 창·투명/드래그/DPI/잠금/display-off/절전/복귀/핸들·메모리 증가와 기준선은 실제 PC에서 확인해야 A-2 완료다. A-3은 실제 원작 대표 패키지와 Windows를 연결해 원작 anchor·그림자·wall time과 통합 검사를 수행한다.

Windows 결과 CSV 요약은 다음과 같다. 기본 warm-up60초, peak는 시작 시점까지 포함한다. 결과가 없으면 수치를 추정하지 않는다.

```bash
python3 tools/summarize_metrics.py 실제경로/walk.csv --output walk-summary.json
```

## 2026-10-13 원작 입력 공유

사용자가 원본 ROM까지 포함한 입력 전체 보관을 명시했다. 원작 MDPA는 `assets/original/runtime/asset-0/asset.mdpa` 또는 `asset-1/asset.mdpa`, 전체 재추출 입력은 `assets/original/source/user-supplied.nds`다. 모든 ROM 파일·system binary 압축본과 해시 목록도 source/에 있다. 추가 업로드 없이 이 경로로 A-3을 진행한다. 전체 입력 보관은 전체 종/폼 변환·재생 pass를 뜻하지 않는다.
