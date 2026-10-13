# A-1 제품 구현 인수인계

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
