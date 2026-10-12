# 현재 상태

## 현재 단계
기술 조사·자료 이전·구현 전 문서 보완 완료. 제품 구현 착수 전.

## 현재 구현 상태
저장소: https://github.com/ahntaeyoung80-coder/MysteryDungeonPet
보고서 9개: docs/research/. 조사 검사 자료 9개: docs/research/checks/.
이전 개발 명세 원본: docs/archive/ (후속 결정 이전 자료).
좌표·표시 영역, 배치 종료 보장, A-1/A-2/A-3 순서 및 층 인덱스 검사 기준을 관련 문서에 반영했다. 작업 규칙·소개·결정 문서의 손실된 한글을 현재 확정된 내용으로 재작성했다.
실행 코드 및 개발 의존성 없음. 실제 ROM 전체 추출과 Windows 실행·성능 검증은 미완료.

## 진행 중
문서 검증 및 실제 git push 완료. 보완 커밋 b02622a가 main에 반영됐다. 다음 작업은 1차 개발 실행 명세 확정이다.

## 다음 작업 / 재개 지점
1. docs/research/00_RESEARCH_STATE.md와 IMPLEMENTATION_HANDOFF.md를 먼저 읽는다.
2. 합의된 보완을 기준으로 1차 개발 실행 명세를 확정한다.
3. A-1 자산 준비와 A-2 렌더러 준비를 수행하고 A-3 실제 원작 자산 통합 검증으로 완료를 판정한다.

## 알려진 문제
ROM 전체 추출·전체 종/폼 지원·Windows 성능은 검증 대기다. 원작 Pose17 사용은 확정되어 있다.
조사 ZIP 및 checks/package_validation.json은 수정 전 자료의 기록이다. 최신 보고서 해시나 새 실행 결과로 취급하지 않는다.
Codex 클라우드 프로젝트의 UI 연결 설정 자체는 별도로 검증하지 않았다.

## 관련 결정
docs/DECISIONS.md
