# 현재 상태

## 현재 단계
기술 조사 보고서 9개 확보 및 저장소 이전. 제품 구현 착수 전.

## 현재 구현 상태
GitHub 저장소: https://github.com/ahntaeyoung80-coder/MysteryDungeonPet
조사 보고서 원문: docs/research/ (첨부 ZIP의 9개 Markdown 파일 그대로 이전).
실행 코드 및 개발 의존성 없음. 실제 ROM 전체 추출과 Windows 실행·성능 검증은 미완료.

## 진행 중
이번 작업 범위는 기존 보고서 이전. 검토에서 합의한 보완 사항은 원문에 아직 반영하지 않음.

## 다음 작업 / 재개 지점
1. docs/research/00_RESEARCH_STATE.md와 IMPLEMENTATION_HANDOFF.md를 먼저 읽는다.
2. 개발 명세에 좌표·표시 영역 분리, 가용 타일 기반 비복원 배치 및 종료 보장, 자산 준비·렌더러 준비·실제 자산 통합 검증 순서를 반영한다.
3. 1차 개발 실행 명세를 확정한 뒤 구현한다.

## 알려진 문제
이전 세션의 GitHub DNS 오류와 달리 이번 세션에서 git ls-remote와 clone은 성공함.
Codex 클라우드 프로젝트의 UI 연결 설정 자체는 별도로 검증하지 않음.
기존 AGENTS.md, README.md, docs/DECISIONS.md에 대체 문자로 손실된 한글이 있음. 이번 이전에서는 해당 파일을 변경하지 않음.
실행 코드가 없어 빌드·런타임 테스트 미실시. 문서상의 테스트 결과를 이번 세션에서 재실행한 것으로 취급하지 않는다.

## 관련 결정
docs/DECISIONS.md
