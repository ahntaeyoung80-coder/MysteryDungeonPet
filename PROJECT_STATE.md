# 현재 상태

기준: 2026-10-16 KST. 개발 기준은 `work`, 후속 구현은 `fix/party-ai-runtime`.

## 현재 단계

**B — 자율 탐험·동료 전투 안정화(B-1/B-2).** A의 대표 자산·Windows 기반을 재사용한다. A의 원작 시간·일부 실제 OS 검증, C의 활동 연동·전체 지원 감사·장기 성능 검증은 남아 있다. C의 식사·수면·기상은 일부 구현됐다. [단계 정의](docs/DEVELOPMENT_PIPELINE.md), [개발 맥락](docs/PROJECT_HISTORY.md).

## 실행·검증 기준

- `work`는2단계 기준선으로, 기본3층·파이리 동료·녹색 켈리몬·MDES7 실행물을 유지한다.
- 작업 브랜치는 독립 전투·MDES8 쓰기/MDES1~8 읽기, 진행 중 구형 전투 이행을 구현했다. 소스 `8c77431`, 검증된 EXE 갱신 `7f151b5`.
- [현재 구현 CI](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38741809780): Windows native13/13·전체 GUI/설치/루트 실행, Linux ASan/UBSan12/12·Python26/26 성공. 실제 Windows11·장기24시간 검증은 남아 있다.
- 기본 층수·켈리몬 색상·동료 HP 표시는 아직 종전 기준이다. 사용자 실행은 작업 완료 후 최종 안내를 따른다. 저장 삭제는 필요하지 않다.

## 진행 중·바로 다음 작업

3단계 검증과 실행물 반영을 마쳤다. [전투 계약](docs/PARTY_COMBAT_DESIGN.md), [검증·시간 로그](docs/logs/2026-10-15-party-ai-stage3.md).

현재4단계에서 직선 경로·저체력 추적 간격·전투 후 합법 복귀를 수정·검증 중이다. [4단계 로그](docs/logs/2026-10-16-party-ai-stage4.md). 명시 회귀 `pet_companion_tests --party-ai-followup`는15/16 통과이며 직선 우선 R01이 남아 있다. 이어5단계 HP표시/색상/7층/방 중복,6단계 통합·최종 실행물/상태 정리를 진행한다. [항목별 계획](docs/PARTY_AI_WORK_PLAN.md).

관리형 실행 환경은 시작 중이며 shell은 아직 없다. 검증된 GitHub Actions 경로로 계속 작업한다.
