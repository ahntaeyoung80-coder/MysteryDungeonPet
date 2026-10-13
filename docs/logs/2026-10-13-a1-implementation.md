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
