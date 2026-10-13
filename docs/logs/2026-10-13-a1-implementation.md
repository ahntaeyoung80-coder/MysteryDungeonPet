# A-1 구현 작업 로그

시간은 Asia/Seoul(KST) 기준이다. 시작 상태: `work` 브랜치, 변경 없음, 제품 코드 없음.

## 예상 시간과 작업 단위

| 단계 | 예상 작업 시간 | 상태 |
| --- | --- | --- |
| A-1.1 고정 의존성 설치/import·ROM 식별 | 10~20분 | 진행 중 |
| A-1.2 던전/MD/item catalogue·CDF·참조 검사 | 15~25분 | 대기 |
| A-1.3 3 WAN 병합·시트/atlas·manifest·지원 보고서 | 25~40분 | 대기 |
| A-1.4 fixture·재현성·오류 보호·문서/원격 반영 | 10~15분 | 대기 |
| A-2 Windows 빌드·창·재생·계측 | 90~150분 | 미착수 |
| A-3 실제 ROM/Windows 통합 검사 | 45~90분 | 미착수 |

ROM/Windows 접근 대기 시간은 제외한다. 실제 결과에 따라 추정치를 갱신한다.

## 소단위 수행 기록

- 문서 확인 완료: AGENTS/PROJECT_STATE/pipeline/DECISIONS와 A-1 관련 조사 계약 확인. 기존 조사 checks와 ZIP는 변경하지 않는다.
- 환경 확인 완료: Linux/Python 3.12.14, ROM·Windows 실행 자원 미확인. 고정 SkyTemple 커밋 다운로드 시작. 네트워크 정책 enforced, GitHub/PyPI 접근 허용.
- 입력 요청: ROM 로컬 경로와 Windows 실행 접근을 한 질문으로 요청. 대기 중 독립 구현 진행.
- 사용량: 계정 잔여 사용량·리셋 시각 조회 도구 없음. 정확한 한도 예측 불가. 한도 경고/도구 제한 시 구현 범위·검증·실행 명령을 인수인계에 남기고 중단.

## 현재 재개 지점

고정 소스 `5ed359a2560a1f04666b9ea02b71649b89cb16d7` checkout, 가상환경 설치 및 파서 import smoke. 아직 제품 구현·원작 gate 통과 없음.

## 10:27 KST 체크포인트

- A-1.1: 고정 VCS 커밋 설치 및 `python -m tools.mdpet smoke` pass. 직접·전이 버전 고정; GTK 미사용.
- A-1.2: catalogue/CDF/index/reference 함수 구현. 일반/비영 start_after 첫·끝 선택, category별 조건부/guaranteed 분리. 테스트 작성 중.
- A-1.3: 3 WAN 병합→export→atlas 구현. raw WAN 방향별 tick/flag/offset 별도 보존. CLI fixture 생성/validate pass(75 frames). 원작 경로는 ROM 부재로 pending.
- 장애: connectivity offline으로 파일/명령 응답 약 4분 지연. 미실행 extractor 저장과 VCS 설치를 파일 대조 후 재개. 데이터 소실 확인되지 않음.
- ROM: 사용자 .nds 첨부는 32MiB 전송 제한으로 실패, 내부 hash/edition 미확인. ZIP 45MB 응답을 받아 ZIP 24M 분할/전체 조각 첨부 안내. 원본을 구하거나 대체 다운로드하지 않음.
- 재개: 파일 오류/alias/분포/재현성/실패 보호 테스트. A-1 원작 gate·원작 wall time·Windows는 pending.

## 10:30 KST 분석 전 인수인계

- A-1.2 소단위 검사 pass: CDF/index/category/normal-MH/BIN_PACK.
- A-1.3 fixture alias/marker/방향/tick/RGBA 검사 pass. 원본 WAN RGBA 대조 미실행.
- A-1.4 10 tests pass: 오류/파일/버전/재현성/기존 출력 보호/unsupported ROM 실패 보고. 원작/Windows pending.
- 사용자 잔여 사용량 부족 안내를 받고 분할 ZIP 분석 전에 docs/A1_IMPLEMENTATION_HANDOFF.md/PROJECT_STATE.md 갱신. .zip/.z01은 아직 미다운로드/미분석.
- 사용량 리셋 후 재개: git status/고정 smoke 확인 → 사용자 분할 ZIP 처리/ROM 식별 → 미지원이면 해당 실패 위치부터. A-2 미착수.

10:59 KST

## ROM 분석 후 체크포인트

- 분석 전 구현 커밋 `4db2628f867e14d54630cfcddec3be1e54961fb5` push 성공. `git ls-remote origin refs/heads/work` SHA 일치 확인.
- 사용자 `.z01` 25,165,824bytes와 `.zip` 20,955,244bytes 다운로드 완료. `zip -s 0 local/rom/user.zip --out local/rom/user-joined.zip`으로 결합.
- ZIP 중앙 CP949 이름/Unicode extra와 local UTF8 flag가 달라 Python 기본 해제 실패. local UTF8 이름이 central Unicode 이름과 동일함을 검증하고 메모리의 `ZipInfo.orig_filename`만 정렬해 해제. 원본 ZIP 수정 없음. 해제 EOF CRC pass.
- 실제 ROM `local/rom/user.nds` 91,394,048bytes, SHA-256 `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`.
- 내부 `C2SE`, ARM9[0xE:0x10]=0x271A, 고정 설정 식별 `EoS_NA`. filename K는 언어 지원 증거가 아님.
- 실제 MAPPA 100 groups / MD 1,155 / item_p 1,400 rows 파싱 성공.
- strict 추출 `local/a1-rom-02.failure.json`: text_e.str/pmd2_decode unknown byte141에서 실패. 전후 ROM 해시 일치. language=English는 구성 언어이고 실제 한국어 매핑은 미확인.
- `--allow-undecoded-text` 경로 추가: raw STR 포인터/종단 검사 후 공식 raw API 사용, 이름 bytes(rawNameHex)/오류 보존, 실제 language=null/textSupport=undecoded. strict 기본값 유지. 인코딩을 임의 CP949로 바꾸지 않음.
- 원문 범위 검사 추가 후 11 tests pass. `local/a1-rom-03 --allow-undecoded-text` 실제 추출 실행 중; 결과 미확정.
- 새 재개 지점: 실행 결과/실패 보고 확인 → catalogue/WAN 변환의 첫 실제 실패 수정 → 대표 동작/재실행 검사. ZIP 다운로드·식별을 처음부터 반복하지 않는다.

## 11:05 KST 소단위 완료

- STR 실제 ROM에는 명시적 EOF pointer가 없는 변형이 있음. pointer table/원문 범위를 검사하며 공식 raw API 사용. 양쪽 형식 regression 포함 11 tests pass.
- 실제 A-1.2: 180 정의/100 MAPPA/1155 MD/1400 item 연결. 기본 선택을 여러 층의 비영 start_after로 강화: d0/local0→group0, d15/local0→group8, d15/local6→group14.
- 전체 정의 중 범위오류 2개와 upstream validator 경고는 errors에 보존. 전체 지원 pass 아님.
- 실제 A-1.3: MD1/sprite0와 MD4/sprite4 자동 선정. MD2/3 Eat/Wake/Pose 누락은 support 오류로 보존. 348 선택프레임과 원본 3 WAN 484 source-frame 비교 pass. merge 전 원본 deepcopy→merge→atlas RGBA/marker/flag/tick 검사.
- source transformVersion/sourceHash/실제 핵심 의존성 버전 추가. 최종 동일 입력 2회 재현성 검사 시작.
- 재개: local/a1-rom-final-01 결과 확인 → 동일 입력 final-02 → hash/지원/요약 기록 → commit/push. 실제 한글 문자 매핑·원작 시간·Windows·전체 지원은 pending.

11:10 KST

## 최종 체크포인트 — 다음 실행 기준

사용자가 잔여 사용량이 적다고 알려 A-1의 대표 검증 단위에서 체크포인트를 확정했다. 수치·리셋 시각은 확인할 수 없다. A-2 대규모 신규 구현은 아직 착수하지 않았다.

### 작업 영역 어디까지 구현했는가

- 작업 영역 `/workspace/MysteryDungeonPet`, 브랜치 `work`. `tools/mdpet/`의 Extractor/Packer/CLI/fixture/파일 검증 구현 완료.
- A-1.1: 고정 소스·직접/전이 의존성 설치/import와 실제 ROM hash/판본 식별 pass.
- A-1.2: 180 정의/100 MAPPA groups/1,155 MD/1,400 item 연결. 선택 profile 3개(d0 local0, d15 local0/6)의 local/group/참조/CDF/context/category 검사 pass.
- A-1.3: 자동 선정 MD1/sprite0와 MD4/sprite4의 Walk/Idle/Eat/Sleep/Wake/Pose+EventSleep/Laying pass. 348 atlas frames와 merge 전 3 pack 원본 WAN 484 source frames의 픽셀·marker·flag·tick·shadow 보존 pass. MD2/3 필수 clip 누락은 support에 기록.
- A-1.4: 11 tests pass. 동일 ROM/선택/도구로 2회 추출한 catalogue/manifest/support/errors/atlas SHA-256 일치. 원본 ROM 전후 해시 일치. 잘린 파일·alias·rect·버전/참조 오류와 기존 출력 보호 검사 pass.
- **A-1 전체 완료라고 표시하지 않는다.** 문자 매핑 1 unresolved, 일반 정의 범위실패 2, upstream validator 경고 33, 전체/도장/종·폼 미감사. 원작 wall time과 Windows gate pending. 대표 자산·데이터 준비만 검증됐다.
- atlas PNG 합계 32,363bytes / decoded 508,928bytes. 런타임 메모리·설치 크기 측정값은 아니다.
- 실제 결과 요약 `docs/validation/2026-10-13-a1-summary.json`, 실행 안내 `docs/A1_USAGE.md`. 원본·게임 이미지·생성 catalogue/atlas는 `local/`에만 두고 커밋하지 않는다.

### 사용량 리셋 후 어디서 다시 시작하는가

**다음 독립 구현 지점은 pipeline A-2.1: C++17/MSVC/CMake 빌드 골격과 A-1 fixture/원작 선택 패키지를 읽는 소비 형식 확정이다.** 현재 네이티브 코드/Animator/AssetStore/WindowHost/Renderer는 전부 미착수다. 큰 JSON 엔진을 선행 추가하지 않고 compact manifest 소비와 오프라인 감사 JSON의 연결을 검증한다. A-2 예상 90~150분, A-3 실제 통합 45~90분(Windows 접근 대기 제외).

재개 전에는 아래 상태만 대조한다. 이미 성공한 추출을 이유 없이 반복하지 않는다.

```bash
cd /workspace/MysteryDungeonPet
git status --short
.venv/bin/python -m tools.mdpet smoke
.venv/bin/python -m tools.mdpet validate local/a1-rom-final-01
```

현재 실제 결과: `local/a1-rom-final-01`, `local/a1-rom-final-02`. 최신 manifest SHA-256 `c657d24ce3da04c8c20a289c9a2f5ee493838a9ab2471ad31723406b7b294c8d`.
원본 ROM: `local/rom/user.nds`, SHA-256 `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`.

문자 지원을 이어갈 경우 `local/a1-rom-02.failure.json`의 `text_e.str/pmd2_decode` unknown byte141과 현재 `checked_raw_strings`/rawNameHex부터 시작한다. 실제 `language=null`, 구성 `English`, 정확한 문자 매핑은 unresolved다. 지원되지 않는 문자들을 임의 CP949/영문으로 대체하지 않는다.
Windows 실제 Release 실행/잠금/절전/성능 접근은 아직 없고 사용자에게 최초 묶음 요청한 상태다. Linux/크로스빌드 결과만으로 A-2/A-3 통과를 선언하지 않는다.

환경이 교체돼 로컬 원본이 없어졌을 때만 첨부를 다시 받는다: ZIP `file_000000008198820b8705eddfd45e54b3`, z01 `file_0000000046a8820b8c966d70382897e2`. 두 조각은 각각 32MiB 미만이다. 기존 환경에서는 재다운로드/ZIP 결합/ROM 식별을 반복하지 않는다.
