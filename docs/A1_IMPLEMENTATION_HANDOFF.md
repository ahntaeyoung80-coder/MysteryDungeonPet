# A-1 제품 구현 인수인계

**최신 재개 기준은 아래 「최종 체크포인트」다. 앞부분은 분석 전/중의 이력이며 최신 상태를 우선한다.**

2026-10-13 KST. 사용자 요청으로 **첨부 ROM 분석 전에 작성한 체크포인트**다.
사용자가 잔여 사용량이 적다고 알렸다. 계정 사용량/리셋 시각 조회 도구는 없으므로 수치나 리셋 시간을 추정하지 않는다.

## 작업 영역과 구현 범위

- 저장소 `/workspace/MysteryDungeonPet`, 작업 브랜치 `work`.
- A-1 독립 구현과 fixture 검증. A-1 원작 gate pending. A-2/A-3/B/C는 미착수.
- `requirements-offline.txt`: Python 3.12 직접/전이 버전 고정. SkyTemple 조사 커밋 `5ed359a2560a1f04666b9ea02b71649b89cb16d7` VCS 설치.
- `tools/mdpet/common.py`: SHA-256/canonical JSON, 경로 검사, 임시 출력 후 확정, 기존 출력 덮어쓰기 거부, 실패 보고.
- `tools/mdpet/catalogue.py`: 비영 start_after 포함 local/group floor 검사, CDF 차분/GCD, normal/MH 분리, 종료/dummy/케클레온 분리, ROM 카테고리별 conditional 아이템과 guaranteed 보존.
- `tools/mdpet/extractor.py`: import smoke, BIN_PACK count/offset 검사, ROM 식별·전후 hash, 일반 던전 정의/MD/item/문자열 연결, 일반층+비영 시작 첫/끝 선택, signed spriteIndex로 3 WAN 읽기/병합/export. 필수 동작 지원 대표 MD 1~2개 자동 선택 경로 구현. **실제 ROM 경로는 미실행**.
- `tools/mdpet/packer.py`: XML/PNG→straight RGBA atlas/manifest/support/validation, alias closure, 1/8행, 원본 방향별 tick/flag/offset/shadow, 공통 crop/anchor/marker, 재조립 RGBA hash, 버전·참조·rect 검사.
- `tools/mdpet/fixture.py`: 직접 생성한 도형 fixture. 원작 그래픽·가짜 ROM/MD/sprite ID 없음.
- `tools/mdpet/__main__.py`: 실제 CLI `smoke`, `fixture`, `pack`, `validate`, `extract`. 실패 시 `<out>.failure.json`에 식별/지원/오류 보고와 함수 위치 보존.
- `tests/test_a1.py`: 10개 자동 검사. `.gitignore`에 `local/`, ROM/save 파일 제외.
- 기존 연구 checks/조사 ZIP는 보존. 생성 자산은 `local/`에만 존재하며 커밋하지 않는다.

## 실행된 검증

환경: Linux/Python 3.12.14. `.venv` 고정 VCS 소스 설치 성공. GUI/GTK 미사용.

```bash
cd /workspace/MysteryDungeonPet
.venv/bin/python -m compileall -q tools
.venv/bin/python -m tools.mdpet smoke --report local/a1-smoke.json
.venv/bin/python -m tools.mdpet fixture --out local/a1-fixture-01
.venv/bin/python -m unittest discover -s tests -v
```

- 위 명령 성공. smoke는 설치된 VCS commit까지 검사한다.
- fixture 1 asset/75 실제 프레임 pass. manifest SHA-256 `1cffb1521e8722caccaacb2be61a8316d6d379488dbc1a0ac51c6dc753b2eeb2`.
- 10 tests pass: CDF 중간0/동일/빈/비단조, 비영 시작 첫/끝, category/guaranteed, normal/MH/특별행, BIN_PACK 잘림/offset, alias/cycle, 1/8행/방향별 tick·짧은 방향, marker 겹침/RGBA 재조립, 버전/rect/path/atlas/hash 거부, 출력 보호, 합성 unsupported ROM 실패 보고/원본 불변.
- 테스트 내 두 패키지의 manifest/atlas 해시 일치.
- upstream `getdata` deprecation 경고 1건. 기능 실패는 없으며 Pillow 12.3.0 고정.

## 첨부와 자원 — 분석 전

- 최초 `.nds` 첨부는 전송 도구 32MiB 제한으로 실패. ROM bytes/hash/판본 미확인.
- 새 첨부 `포켓몬 하늘의 탐험대(K).zip` / `.z01`은 **아직 다운로드·결합·내부 분석하지 않았다**.
- 파일명은 내부 판본 지원 증거가 아니다. 고정 SkyTemple 설정은 한국어판 지원을 보장하지 않는다. 임의 NA 주소를 적용하거나 다른 ROM으로 대체하지 않는다.
- Windows/MSVC/SDK/실제 실행 접근 없음. Linux 검사로 Windows gate를 통과시키지 않는다.
- 환경 connectivity offline 약 4분 발생. 파일 대조 후 미실행 extractor 저장/VCS 설치부터 재개했으며 이후 테스트 성공.

## 미완료와 한계

1. 사용자 분할 ZIP 결합/로컬 해제, 원본 hash/edition/language 식별. 미지원이면 식별값·실패 위치·해시 기록.
2. 실제 ROM catalogue/3 WAN/export/필수 동작·픽셀·marker/재실행 검사. 실제 판본/API 실패를 근거로 수정.
3. **현재 pixel pass는 시트→atlas 재조립**이다. 원본 WAN→export RGBA 독립 대조는 아직 없으며 원작 픽셀 동일성은 pending.
4. 전체 ROM 층/도장·종/폼 감사 pending. dojo 특례는 미구현. 대표 검사로 G1/G2 전체 pass를 주장하지 않는다.
5. 런타임 소비 형식 미확정. JSON은 오프라인 감사 계약. A-2에서 compact 소비 형식·alias rawDirections·공유 fixture를 함께 검증한다.
6. 원작 wall time/tickHz 대조 pending. 잠정 60Hz는 별도 보존.
7. A-2 빌드/WindowHost/AssetStore/Animator/Renderer/계측 미착수. A-3 Windows 실측 pending.

## 사용량 리셋 후 반드시 여기서 재개

`PROJECT_STATE.md`와 이 문서의 최신 체크포인트를 읽고 실제 파일/커밋을 대조한다. **조사나 A-1 구현을 처음부터 반복하지 않는다.**

```bash
cd /workspace/MysteryDungeonPet
git status --short
.venv/bin/python -m tools.mdpet smoke
.venv/bin/python -m unittest discover -s tests -v
```

다음 실행 지점은 **사용자 `.z01`+`.zip` 다운로드/분할 ZIP 처리 및 ROM 식별**이다. 실제 경로를 확보하면:

```bash
# 재개용 미실행 명령. 실제 사용자 ROM 경로로 바꾼다.
.venv/bin/python -m tools.mdpet extract --rom 'local/rom/user.nds' --out local/a1-rom-01
```

미지원이면 `<out>.failure.json`과 `extractor.py:extract`의 `get_ppmdu_config_for_rom` 식별 실패부터 확인한다. 판본 설정이 검증되기 전 원작 A-1은 pending 유지. 독립 다음 작업은 pipeline A-2.1 C++17/MSVC 빌드와 fixture 소비 계약이다.

## 단계별 예상 시간

| 단계 | 최초 예상 | 현재 남은 범위 |
| --- | --- | --- |
| A-1.1 | 10~20분 | 의존성 완료; ROM 식별 5~15분(전송 대기 제외) |
| A-1.2 | 15~25분 | 독립 검사 완료; 실제 catalogue 10~25분, 판본 실패 시 재산정 |
| A-1.3 | 25~40분 | fixture pass; 실제 WAN/필수 동작 20~40분, 판본 실패 시 재산정 |
| A-1.4 | 10~15분 | fixture 오류/재현성 pass; 실제 ROM 재실행/문서 반영 대기 |
| A-2 | 90~150분 | 미착수, Windows 접근 대기 별도 |
| A-3 | 45~90분 | 미착수, 실제 ROM/Windows 필요 |

커밋/push 결과와 이후 ROM 분석은 아래에 추가한다. 인수인계 작성 자체는 원작 검증 완료가 아니다.

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
