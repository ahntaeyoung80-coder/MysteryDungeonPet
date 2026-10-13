# 현재 상태

## 현재 단계
Codex 개발 파이프라인과 1차(A) 실행 명세 작성 완료. 제품 구현 착수 전.

## 현재 구현 상태
저장소: https://github.com/ahntaeyoung80-coder/MysteryDungeonPet
개발 진입점: docs/DEVELOPMENT_PIPELINE.md. 전체 A/B/C 개요와 A-1/A-2/A-3 작업·산출물·검증·완료 조건을 정의했다.
B-2 자동 전투는 선택 검토 단계다. B-1 완료 시 사용자와 추가 여부·범위를 확정하기 전에는 구현하지 않는다.
조사 보고서 9개: docs/research/. 과거 검사 자료 9개: docs/research/checks/.
이전 개발 명세: docs/archive/ (현재 실행 기준과 구분).
제품 코드·추출기·packer·Windows 실행 파일은 아직 없다. 실제 ROM 전체 추출·전체 종/폼 및 Windows 성능은 미검증이다.

## 진행 중
문서 작성과 진입 경로 정리 완료. 다음 작업은 A-1 자산 준비와 A-2 렌더러 준비다.

## 다음 작업 / 재개 지점
1. docs/DEVELOPMENT_PIPELINE.md §4~§7을 읽는다. 필요한 조사 소스·계약만 추가 참조한다.
2. 고정 오프라인 의존성 import와 최소 Extractor/Packer, 같은 fixture 계약의 Windows 빌드·렌더러를 준비한다.
3. 사용자 ROM/Windows가 없으면 가능한 독립 작업을 수행하고 통합 gate는 pending으로 남긴다.
4. 실제 원작 자산과 Windows 환경으로 A-3을 검증한 후 A 완료를 판정한다.

## 알려진 문제
실제 ROM·Windows 검증은 대기 상태이며 문서 작성이나 공개 fixture 검사를 제품 완료로 취급하지 않는다.
원작 Pose17 사용은 확정되어 있다. 부가 효과·계단 그림 매핑은 후속 단계에서 확인한다.
조사 ZIP와 checks/package_validation.json은 수정 전 기록이며 최신 보고서 해시가 아니다.

## 관련 결정
docs/DECISIONS.md
