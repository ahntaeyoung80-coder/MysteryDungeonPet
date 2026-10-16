# 현재 상태

기준: 2026-10-16 KST. 개발 기준은 `work`, 후속 구현은 `fix/party-ai-runtime`.

## 현재 단계

**B — 자율 탐험·동료 전투 안정화(B-1/B-2).** A의 대표 자산·Windows 기반을 재사용한다. A의 원작 시간·일부 실제 OS 검증, C의 활동 연동·전체 지원 감사·장기 성능 검증은 남아 있다. C의 식사·수면·기상은 일부 구현됐다. [단계 정의](docs/DEVELOPMENT_PIPELINE.md), [개발 맥락](docs/PROJECT_HISTORY.md).

## 실행·검증 기준

- `work`는2단계 기준선으로 기본3층·파이리 동료·녹색 켈리몬·MDES7 실행물을 유지한다.
- 작업 브랜치는 독립 전투·직선 최단 경로·목표 상실 시 전투/대기·합법 복귀·MDES8 쓰기/MDES1~8 읽기와 구형 진행 중 전투 이행을 구현했다. 동료 HP 숨김·녹색 우호/보라색 적도 검증했다. 소스 `8d4a1cd`, EXE/보라색 자산 갱신 `aa4cf05`.
- [현재 구현 CI](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38825676009): Windows native14/14·후속23/23·GUI/설치/루트 실행, Linux ASan/UBSan13/13·Python26/26 성공. HP1/HP5 추적 이동 프레임·저장 복원·구형 저체력200배치를 검증했다.
- 기본 층수는 검증 기준3층이며7층/구형 저장 이행을 구현·검증 중이다. 관계별 색상과 동료 HP 숨김은 실제 픽셀/복원 검사를 통과했다. 실제 Windows11·원작 시간·장기24시간 검증은 남아 있다. 저장 삭제는 필요하지 않다.

## 진행 중·바로 다음 작업

4단계 검증과 실행물 반영을 마쳤다. [전투·이동 계약](docs/PARTY_COMBAT_DESIGN.md), [4단계 변경·시간 로그](docs/logs/2026-10-16-party-ai-stage4.md), [검증 증거](docs/validation/2026-10-16-party-ai-stage4.json).

현재5단계 동료 HP표시·녹색/보라색 자산·기본7층/기존3층 저장·방 중복 유형을 수정·점검 중이다. [5단계 로그](docs/logs/2026-10-16-party-ai-stage5.md). 이어6단계 통합·최종 실행물/현재 상태 정리와 `work` 반영을 진행한다. [항목별 계획](docs/PARTY_AI_WORK_PLAN.md).

관리형 shell은 아직 없으며 검증된 GitHub Actions 경로로 작업한다.
