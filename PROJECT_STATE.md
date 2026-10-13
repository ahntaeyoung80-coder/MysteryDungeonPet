# 현재 상태

## 현재 단계
A-1은 pipeline §5의 대표 데이터·원작 자산 준비 범위에서 완료. A-2 네이티브 구현·A-1 소비 계약·독립 검증·Windows 크로스 빌드 완료. 사용자의 Windows11 기존 실행 기록을 수집·점검했고 다음 재개 지점은 A-3 통합 검증이다. 추가 수동 테스트 없이 진행하되 계측 보완·원작 통합 및 기록에 없는 OS 사건은 pending으로 유지한다. A 전체/B/C 미완료.

## 구현·실행된 검사
- 기존 `tools/mdpet/` Extractor/Packer/CLI와 대표 ROM 검증을 보존. 최신 origin/work `4d6ed6b`를 받아 A-1을 재구현하지 않았다.
- A-1 증거: C2SE/0x271A→EoS_NA, ROM91,394,048bytes/hash `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`, 원본 불변. profile3, MD1/sprite0·MD4/sprite4, 필수6+선택2 clip/348 atlas frames, 원본3 WAN→merge→atlas484 source frames 비교pass, 2회 추출hash 일치. 상세는 기존 A1 로그/summary.
- `src/core/` Package/Animator/Surface, `src/windows/` WIC AssetStore/WindowHost/DIB Renderer/CSV 계측/창 state 구현.
- `tools/compile_runtime_package.py`: A-1 audit→MDPA 소비 형식. alias 자기 tick/flag/marker와 대상 atlas 공유. 합성 A-1→native144 frames 대조pass, 변환 재현성/기존 출력 보호pass.
- Python13 tests pass, C++10,716 checks/전체 잘린 prefix/10,000 seeded mutations pass, ASan/UBSan pass, Windows x64 MinGW Release compile/link pass.
- 직접 생성 fixture/PNG/manifest·재현 검사, MSVC/native WIC와 8개 오류 package의 CI, Windows 실행 ZIP/PowerShell 절차 추가. Windows Server2025/MSVC19.51 Release native core/WIC/PBGRA·cache·8개 오류 package CI pass(run38339188747, 구현680f4c1). 사용자 Windows11에서 WIC/캐시/Hide/100회 전환과 합성 자산 CPU·메모리 기록을 확인했다. 실제 OS 사건의 명시 증거는 별도 미검증.

- 사용자 측정: Walk 한 코어 평균CPU0.1594%, warm-up 후working set최대14,258,176bytes/private2,461,696bytes. Switch100회/decode1회/GDI2개 고정, Hide추가제출0. 상세는 `docs/validation/2026-10-13-a2-user-review.json`.
- 점검 문제: 방향/동작/frame/장면좌표 로그 없음, 종료 후창정보가DPI0/위치0,0으로 기록됨, empty에서도623marker로 Animator 실행. 코드 수정은 사용자 지시로 되돌려 미반영.

## 자원·지원 한계
사용자 PC는 Windows11로 확인. 연결된 실행기는 Linux6.18/Debian13이며 사용자 PC의 명령 실행/GUI 조작 도구·연결 terminal이 없다. 실제 Windows11 실행은 로컬 Codex 프로젝트 또는 사용자의 스크립트 실행이 필요하다.
환경 교체로 이전 `local/a1-rom-final-01/-02`·ROM은 현재 환경에 없다. 기존 성공 ROM 추출 기록을 이번에 재검증했다고 취급하지 않는다. 이번 연결 검사는 재생성한 합성 A-1 fixture만 사용했다. 원본 ROM/자산 재다운로드·재추출은 수행하지 않았다.
기존 `text_e.str` unknown byte141은 rawNameHex/actual language=null을 유지하며 unresolved. 전체/도장/종·폼 감사, 원작 wall time, 그림자 표시, 실제 lock/display-off/suspend·DPI변경의 명시 증거와 장기 자원 수명은 pending. 합성 자산의 제출된 성능 기록은 확인했지만 최종 제품 성능으로 확대하지 않는다.

## 다음 실행 지점
1. `docs/logs/2026-10-13-a2-user-review.md`와 측정 JSON을 읽고 기존 사용자 결과를 재사용한다. A-3 착수 전에 새 수동 테스트를 요청하지 않는다. 이번 저장은 점검 기록만 반영하며 제품 코드·자산은 변경하지 않는다.
2. A-3 첫 작업에서 방향·동작ID·frame·장면 좌표 로그 추가, 종료 전 마지막 측정, empty의 Animator 중단을 수행한다. 마지막 정상 위치/DPI와 기록된 재생 상태의 일치를 자동 검사한다.
3. 사용자 요구인200×200 실제px 고정·카메라 컷/조작 제거를 반영한다. 현재 코드의100/200 선택·카메라 메뉴는 아직 제거되지 않았다. 합성 도형 개선은 생략한다.
4. 이전 원작 선택 package를 확보해 `compile_runtime_package.py --package ... --out 새경로`로 변환하고 실제 원작 자산·시간·anchor/shadow를 통합 검증한다. 미검증 항목을 pass로 바꾸지 않으며 A 전체 완료는 통합 결과 확인 후 판정한다.
5. B-2 자동 전투는 B-1 완료 후 선택 단계이며 미확정 상태로 구현하지 않는다.

## 기록
`docs/A1_IMPLEMENTATION_HANDOFF.md`, `docs/A1_USAGE.md`, 기존 A1 로그/검증summary, `docs/A2_USAGE.md`, `docs/implementation/MDPA_FORMAT.md`, `docs/logs/2026-10-13-a2-runtime.md`, `docs/validation/2026-10-13-a2-summary.json`, `docs/logs/2026-10-13-a2-user-review.md`, `docs/validation/2026-10-13-a2-user-review.json`.
ROM/게임 자산/생성 원작 package는 local/에만 두고 Git 제외. 연구 checks/ZIP 보존. push 결과와 실제 Windows 실행 판정을 별도로 기록한다.
