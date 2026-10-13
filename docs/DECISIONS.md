# 설계 결정

- 외부 Git 저장소와 동기화하는 프로젝트는 해당 저장소를 기준으로 관리하며 별도 사본을 중복 보관하지 않는다.
- 기존 코드·원작 데이터·리소스 재활용과 최소 자원 사용을 우선한다. 런타임에 LLM·에뮬레이터·전체 게임 엔진을 상주시킬 필요는 없다.
- 던전은 시작방·이동방·끝방(계단)의 2~3개 방으로 단순화한다. 원작 출현 종류와 상대 분포는 유지하고 배치 수·생활 주기는 별도 정책으로 둔다.
- 원작 클리어 포즈 WAN17/Pose를 사용한다. 포즈 선택을 미확정 요구사항으로 되돌리지 않는다.
- 오프라인 추출·변환에는 SkyTemple 도구를 사용하고, 런타임 기본안은 C++17/Win32/WIC이다. 성능 목표는 Windows 실측으로 판정한다.

## 2026-10-13 — 구현 전 보완

- 던전 논리 좌표, 장면 픽셀 좌표, DIB 표시 영역과 Windows 화면 좌표를 분리한다. 작은 창 내부의 표시 영역만 합성하며 탐험 이동으로 데스크톱 창을 이동시키지 않는다.
- 출현 위치는 배치 가능한 타일 집합에서 비복원 추출한다. 배치 수는 정책 예산과 가용 타일 수 이하로 제한하고, 소진·초과 요청을 기록한 뒤 종료한다. 원본 확률 가중치는 변경하지 않는다.
- Phase A는 A-1 자산 준비, A-2 렌더러 준비, A-3 실제 원작 자산 통합 검증으로 나눈다. A-1/A-2는 독립 개발 가능하며 A-3에 양쪽 결과가 필요하다. 테스트 자산만으로 Phase A 완료를 판정하지 않는다.
- groupFloor=start_after+localFloor 규칙과 API의 그룹 기준 인덱싱을 유지한다. start_after가 0이 아닌 던전을 포함해 첫층·끝층 범위를 검사한다.

## 2026-10-13 — 개발 파이프라인

- docs/DEVELOPMENT_PIPELINE.md를 제품 개발의 진입점으로 사용한다. 조사 인수인계는 계약·근거 자료로 연결하고 이전 명세는 보관본으로 유지한다.
- A는 대표 개체의 동작 재생·Windows 기준선, B는 자율 탐험·획득/클리어 저장, C는 생활·활동 연동·전체 지원 감사를 담당한다. A의 샘플 통과를 G1/G2 전체 검증 통과로 확대하지 않는다.
- 최종 CPU 1% 목표는 한 코어 기준으로 평가하고 전체 논리 코어 정규화 값도 함께 보고한다. RAM은 정상 시나리오 working set/private bytes 최대를 각각 30,000,000bytes와 비교하고 transient peak를 별도 보고한다. A에서는 기준선을 확보하고 최종 목표 판정은 C에서 수행한다.

## 2026-10-13 — 선택 자동 전투 검토

- 현재 A/B-1은 비전투·우호 정책으로 구현한다. 자동 전투 B-2는 B-1 완료 뒤 C 전에 검토 가능한 선택 단계이며 구현은 아직 확정하지 않았다.
- B-1 완료 시 사용자와 B-2 추가·제외·보류 여부를 확인한다. 착수 전에 전투 규칙·자산/저장 범위·완료 조건·검증 방법을 확정한다. 미확정 상태로 자동 구현하지 않는다.
- 관계·조우 행동은 작은 정책 경계로 두고 원본 공격/피격 자산의 참조·지원 정보와 저장 버전을 보존한다. 전투용 자산 패키징·HP·AI는 선행 구현하지 않는다. 상세 기준은 DEVELOPMENT_PIPELINE.md §8이다.

## 2026-10-13 — 실제 A-1 입력과 패키징

- 입력 ROM의 실제 식별은 EoS_NA(C2SE/0x271A)이지만 text_e.str 디코딩은 실패한다. 구성 언어와 실제 문자 지원을 분리한다. strict 기본값을 유지하고 명시적 --allow-undecoded-text에서는 이름 null/rawNameHex/오류를 보존한다. 임의 인코딩·번역을 성공으로 간주하지 않는다.
- 공식 STR raw API 앞에서 포인터/범위를 검사하며 실제 ROM의 명시적 EOF entry 없는 형식도 다룬다.
- 원본 WAN은 MergeWan 전에 보존한다. 기존 GeneratePiece/DrawOn으로 원본3pack→merged→atlas의 픽셀/marker/flag/tick을 대조하며 시트→atlas 해시만으로 원본 동일성을 보고하지 않는다.
- 오프라인 선택 package는 schemaVersion/assetVersion1, PNG straightRGBA+감사용JSON이다. 변환 버전/sourceHash/핵심 의존성 버전을 보존하며 런타임 소비 형식은 A-2에서 확정한다.
- 정상 출력 덮어쓰기 대신 새 출력 디렉터리의 임시 생성/검사/rename으로 확정한다. 실패 보고는 별도 파일에 남긴다.

## 2026-10-13 — A-2 소비 계약과 실행 환경

- A-1 audit manifest를 의미 원본으로 유지하고 `compile_runtime_package.py`로 MDPA v1 소비 형식을 만든다. runtime에는 JSON 엔진을 추가하지 않으며 원본 provenance·timing·frame 정보를 함께 보존한다.
- alias는 atlas를 공유하되 원본 alias의 duration/flag/marker/프레임 수를 유지한다. geometry가 다른 alias, 해석되지 않은 rawSpeed, 단일 atlas8MiB 초과는 명시 오류로 처리한다.
- Windows 창은100/200 실제 px, PMv2 aware, 별도 integerScale1..8로 표시한다. DPI 변경은 위치 clamp만 적용하며 비정수 확대를 도입하지 않는다.
- 한 UI 스레드와 one-shot waitable timer, 동일 pixel 재제출 방지, 단일 atlas 캐시부터 시작한다. 검사 모드의 CSV는1초 간격으로 기록한다.
- 연결된 cloud 실행기는 Linux이고 사용자 PC는Windows11다. 현재 도구로 사용자 PC에 직접 실행할 수 없으며, 사용자가 Windows 실행 ZIP/PowerShell 문서대로 직접 검증한다. headless Windows CI와 실제 UI/lock/suspend gate를 구분한다.
