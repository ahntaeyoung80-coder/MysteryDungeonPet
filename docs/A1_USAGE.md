# A-1 추출·패키징 실행

검증 환경: Linux/Python 3.12.14. Windows의 오프라인 도구 실행은 미검증. GUI/GTK 없이 실행하고 ROM·생성 자산은 `local/`에 두며 커밋하지 않는다.

## 설치·독립 검사

```bash
python -m venv .venv
.venv/bin/python -m pip install -r requirements-offline.txt
.venv/bin/python -m tools.mdpet smoke --report local/a1-smoke.json
.venv/bin/python -m unittest discover -s tests -v
.venv/bin/python -m tools.mdpet fixture --out local/my-fixture
.venv/bin/python -m tools.mdpet validate local/my-fixture
```

smoke/tests/fixture/validate는 실제 실행 검증했다. 새 가상환경 설치 명령은 재현 안내이며 현재 환경은 동일 고정 VCS 커밋과 목록 버전으로 설치했다. 다른 환경에서 `.venv`를 복사하지 않고 requirements로 설치한다.

## 실제 ROM

```bash
.venv/bin/python -m tools.mdpet extract --rom local/rom/user.nds --out local/my-rom-package
```

`gameCode`+ARM9 식별값과 고정 SkyTemple 설정으로 판본을 고르고 미지원은 거부한다. 기본 선택은 일반층과 여러 층의 비영 start_after 첫/끝층이다. 대표 MD는 spriteIndex/필수 WAN 지원으로 자동 선정하며 특정 종에 고정하지 않는다.

`--md 1 --md 4` 또는 `--floor 15:0 --floor 15:6`으로 선택할 수 있다. 이는 실제 확인 샘플의 원본 ID이며 층은 0 기반이다. `--all-general-floors`는 전체 일반층 감사 경로지만 전체 실행은 아직 하지 않았다. dojo는 pending이다.

### 번역/개조 ROM의 문자 인코딩

사용자 ROM은 `C2SE/0x271A`→EoS_NA이지만 `text_e.str`이 기존 `pmd2str`로 디코딩되지 않아 strict 추출은 실패했다. 원문과 오류를 남기며 데이터·동작만 준비하려면:

```bash
.venv/bin/python -m tools.mdpet extract --rom local/rom/user.nds --out local/my-rom-raw-text --allow-undecoded-text
```

이 옵션은 이름 `null`, 원문 `rawNameHex`, 실제 `language=null`, 구성 `configuredLanguage=English`, `textSupport=undecoded`로 기록한다. 한국어 문자 매핑은 unresolved이고 임의 CP949/영문 이름 대체를 하지 않는다.

## 출력·실패

- `catalogue.json`: ROM 식별/해시, 일반 정의·MD/item 참조, 선택 층/원본 분포/정수 가중치, 원문 이름, 범위·미실행 목록.
- `manifest.json`와 `assets/<md>/atlas.png`: 선택 animation/frame/방향/crop/anchor/marker/tick/alias, 출처·변환 소스 해시/의존성 버전.
- `support.json`, `errors.json`, `validation.json`, `rom-verification.json`: 누락·경고·검사·전후 ROM 해시.
- `sheets/`: 디버깅용 export/raw-wan/독립 WAN 검증. 런타임 자산·설치 크기와 구분한다.
- fixture는 `sourceKind=test`이고 ROM ID 없이 시험 프레임만 만든다.

기존 `--out`은 덮어쓰지 않는다. 실패하면 임시 출력은 제거하고 `<out>.failure.json`에 단계·함수/모듈·원인·지원/식별 보고와 재개 명령을 남긴다. 새 출력 경로로 재시도한다.

실제 결과: [구현 인수인계](A1_IMPLEMENTATION_HANDOFF.md), 시간·소단위 기록: [작업 로그](logs/2026-10-13-a1-implementation.md). 대표 통과와 원작 시간·전체 지원·Windows 완료를 구분한다.
