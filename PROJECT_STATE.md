# 현재 상태

## 현재 단계
A-1은 pipeline §5의 대표 데이터·원작 자산 준비 범위에서 완료. A-2 네이티브 구현·A-1 소비 계약·독립 검증·Windows 크로스 빌드 완료. 실제 Windows UI/전원/성능 검증 대기로 A-2 완료 판정은 pending, A-3/B/C 미완료.

## 구현·실행된 검사
- 기존 `tools/mdpet/` Extractor/Packer/CLI와 대표 ROM 검증을 보존. 최신 origin/work `4d6ed6b`를 받아 A-1을 재구현하지 않았다.
- A-1 증거: C2SE/0x271A→EoS_NA, ROM91,394,048bytes/hash `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`, 원본 불변. profile3, MD1/sprite0·MD4/sprite4, 필수6+선택2 clip/348 atlas frames, 원본3 WAN→merge→atlas484 source frames 비교pass, 2회 추출hash 일치. 상세는 기존 A1 로그/summary.
- `src/core/` Package/Animator/Surface, `src/windows/` WIC AssetStore/WindowHost/DIB Renderer/CSV 계측/창 state 구현.
- `tools/compile_runtime_package.py`: A-1 audit→MDPA 소비 형식. alias 자기 tick/flag/marker와 대상 atlas 공유. 합성 A-1→native144 frames 대조pass, 변환 재현성/기존 출력 보호pass.
- Python13 tests pass, C++10,716 checks/전체 잘린 prefix/10,000 seeded mutations pass, ASan/UBSan pass, Windows x64 MinGW Release compile/link pass.
- 직접 생성 fixture/PNG/manifest·재현 검사, MSVC/native WIC와 8개 오류 package의 CI, Windows 실행 ZIP/PowerShell 절차 추가. Windows Server2025/MSVC19.51 Release native core/WIC/PBGRA·cache·8개 오류 package CI pass(run38339188747, 구현680f4c1). 사용자 Windows11 UI/전원 검증은 별도 pending.

## 자원·지원 한계
사용자 PC는 Windows11로 확인. 연결된 실행기는 Linux6.18/Debian13이며 사용자 PC의 명령 실행/GUI 조작 도구·연결 terminal이 없다. 실제 Windows11 실행은 로컬 Codex 프로젝트 또는 사용자의 스크립트 실행이 필요하다.
환경 교체로 이전 `local/a1-rom-final-01/-02`·ROM은 현재 환경에 없다. 기존 성공 ROM 추출 기록을 이번에 재검증했다고 취급하지 않는다. 이번 연결 검사는 재생성한 합성 A-1 fixture만 사용했다. 원본 ROM/자산 재다운로드·재추출은 수행하지 않았다.
기존 `text_e.str` unknown byte141은 rawNameHex/actual language=null을 유지하며 unresolved. 전체/도장/종·폼 감사, 원작 wall time, 그림자 표시, 실제 Windows UI/lock/display-off/suspend/CPU/RAM/handle 수명은 pending.

## 다음 실행 지점
1. `docs/A2_USAGE.md`와 `docs/A2_WINDOWS_QUICKSTART.md` 참조. ZIP의 EXE를 직접, 또는 `Run-A2.ps1`을 실제 Windows11에서 실행.
2. 로컬 Codex에서 이어가면 저장된 work 브랜치를 열고 `tools/run_windows_checks.ps1`로 MSVC/core/WIC 확인. `-Interactive`로 기준선·100회 전환 후 실제 drag/DPI/Win+L/display-off/절전 복귀 확인.
3. CSV·environment.json·WIC 보고서·events.log·육안 결과를 수집해 gate 판정 갱신. `tools/summarize_metrics.py`로 warm-up60초 이후 수치 집계.
4. 이전 원작 선택 package를 사용할 수 있으면 `compile_runtime_package.py --package ... --out 새경로`로 변환해 A-3에 연결. 원작 시간/anchor/shadow와 Windows 통합이 pass일 때까지 A 전체 완료로 판정하지 않는다.
5. B-2 자동 전투는 B-1 완료 후 선택 단계이며 미확정 상태로 구현하지 않는다.

## 기록
`docs/A1_IMPLEMENTATION_HANDOFF.md`, `docs/A1_USAGE.md`, 기존 A1 로그/검증summary, `docs/A2_USAGE.md`, `docs/implementation/MDPA_FORMAT.md`, `docs/logs/2026-10-13-a2-runtime.md`, `docs/validation/2026-10-13-a2-summary.json`.
ROM/게임 자산/생성 원작 package는 local/에만 두고 Git 제외. 연구 checks/ZIP 보존. push 결과와 실제 Windows 실행 판정을 별도로 기록한다.
