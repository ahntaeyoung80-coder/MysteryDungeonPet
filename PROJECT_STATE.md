# 현재 상태

## 현재 단계
A-1 대표 실제 ROM 데이터·자산 준비 검증 완료. **A-1 부분 완료**(문자 매핑·전체 감사 pending). 다음 독립 구현은 A-2.1이며 A-2/A-3/B/C 미착수.

## 구현·실행된 검사
- `tools/mdpet/`: 오프라인 Extractor/Packer/CLI/fixture/validator, 3 WAN 병합, alias·1/8행·crop/anchor/marker/tick, 임시 출력·실패 보고.
- `requirements-offline.txt`: 고정 VCS 커밋/의존성. Linux/Python3.12.14 설치/import pass. `tests/test_a1.py` 11 tests pass.
- 실제 ROM C2SE/0x271A→EoS_NA, 91,394,048bytes. hash `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`, 전후 일치.
- 180 정의/100 MAPPA/1155 MD/1400 item, 선택 profile3개(d0 local0, d15 local0/6). MD1/sprite0와 MD4/sprite4 자동 선택, 필수6+선택2 clip/348 atlas frames pass. 원본3 WAN→merge→atlas 484 source-frame RGBA/marker/flag/tick 비교 pass.
- `local/a1-rom-final-01`과 `-02`의 catalogue/manifest/support/errors/atlas 해시 일치. atlas 32,363bytes/decoded508,928bytes(런타임 메모리·설치 크기 측정 아님).
- manifest hash `c657d24ce3da04c8c20a289c9a2f5ee493838a9ab2471ad31723406b7b294c8d`.

## 막힌 자원·지원 한계
`text_e.str/pmd2_decode` unknown byte141. `--allow-undecoded-text`로 rawNameHex/오류를 보존하며 이름과 실제 language는 null이다. 구성언어 English를 실제 문자 지원으로 취급하지 않는다.
일반 정의 범위오류2/upstream validator 경고33을 보존. MD2/3 Eat/Wake/Pose 누락 보존. 전체 층/도장/종·폼 감사, 원작 wall time/tickHz, Windows 실제 빌드/실행·성능 pending.
사용자가 잔여 사용량이 적다고 알려 대표 검증 단위에서 인수인계 확정. 잔여량/리셋 시각 조회 도구 없음.

## 사용량 리셋 후 재개
1. `docs/A1_IMPLEMENTATION_HANDOFF.md` 최종 체크포인트와 `git status --short` 확인. 기존 성공 추출/조사를 반복하지 않는다.
2. 다음 구현은 **pipeline A-2.1 C++17/MSVC/CMake 빌드와 공유 fixture/선택 패키지 소비 형식**. 네이티브 코드·AssetStore/Animator/WindowHost/Renderer 전부 미착수. A-2 예상90~150분, A-3 45~90분(Windows 접근 대기 별도).
3. 기존 원작 패키지 검사: `.venv/bin/python -m tools.mdpet validate local/a1-rom-final-01`. 원본은 `local/rom/user.nds`. 새 추출이 필요할 때만 새 --out과 --allow-undecoded-text 사용.
4. 문자 지원을 이어갈 경우 `local/a1-rom-02.failure.json`과 `checked_raw_strings`/rawNameHex의 실제 인코딩 매핑부터. 문자 미확인으로 다른 원본이나 임의 영문 이름으로 대체하지 않는다.
5. 실제 Windows 실행 접근은 아직 없음. 독립 개발은 진행 가능하나 OS gate는 실제 실행 결과 필요.

## 기록
`docs/A1_IMPLEMENTATION_HANDOFF.md`, `docs/A1_USAGE.md`, `docs/logs/2026-10-13-a1-implementation.md`, `docs/validation/2026-10-13-a1-summary.json`.
원본 ROM·게임 자산·생성 catalogue/atlas는 local/에만 저장, Git 제외. 기존 연구 checks/ZIP 보존. 분석 전 체크포인트4db2628 push/SHA 일치 확인. 최신 commit/push 결과는 작업 로그 참조.
