# 현재 상태

## 현재 단계
A-1 독립 추출/packer 구현과 fixture 검증 완료. **A-1 실제 원작 gate pending**, A-2/A-3 미착수.

## 구현·검증 범위
- `tools/mdpet/`: CLI smoke/fixture/pack/validate/extract, catalogue 정규화, 3 WAN 읽기/병합/export 경로, atlas/manifest/support/errors, 실패 출력 보호.
- `requirements-offline.txt`: 고정 VCS 커밋/의존성. Python 3.12.14/Linux에서 설치·import pass.
- `tests/test_a1.py`: 10 tests pass. 시험 패키지 1 asset/75 frames pass, 두 번 생성 manifest/atlas 해시 일치. 실제 ROM 추출·원본 WAN 픽셀 대조·Windows 미실행.
- 생성 fixture/로컬 자산은 `local/`에 두고 Git에서 제외. 원본 ROM도 제외.

## 현재 자원·막힌 항목
최초 사용자 .nds 첨부는 32MiB 전송 제한으로 실패. 새 분할 ZIP `.zip`/`.z01`이 도착했으며 사용자 요청으로 **분석 전에 인수인계부터 기록한 상태**다. ROM 내부 판본/해시 미확인. Windows 실행 접근 없음.
사용자가 잔여 사용량이 적다고 알림. 잔여량/리셋 시각 조회 도구 없음. 중단 시 완료 범위와 실제 재개 지점을 반드시 남긴다.

## 다음 작업 / 사용량 리셋 후 재개 지점
1. `docs/A1_IMPLEMENTATION_HANDOFF.md`와 `git status --short` 확인. 구현을 처음부터 반복하지 않는다.
2. 사용자 `.z01`+`.zip` 다운로드/결합·로컬 ROM 해제 후 hash/edition/language 식별.
3. `.venv/bin/python -m tools.mdpet extract --rom 'local/rom/user.nds' --out local/a1-rom-01` (실제 경로에 맞춤; 미실행).
4. 미지원이면 실패 보고/판본 식별 기록, A-1 원작 gate pending 유지. 지원하면 대표 catalogue/WAN/필수 동작/재실행 검증.
5. 독립 다음 작업은 pipeline A-2.1 C++17/MSVC 빌드·공유 fixture 소비 형식. A-3/B/C는 미착수.

## 기록
인수인계: `docs/A1_IMPLEMENTATION_HANDOFF.md`.
소단위 로그/예상 시간: `docs/logs/2026-10-13-a1-implementation.md`.
설계: `docs/DECISIONS.md`, 순서: `docs/DEVELOPMENT_PIPELINE.md`.
기존 연구 checks/ZIP 보존. 커밋/push 결과는 작업 로그에 추가한다.
