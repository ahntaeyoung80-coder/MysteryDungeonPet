# 현재 상태

기준: 2026-10-15 KST / 개발·조회 브랜치 `work`.

## 현재 단계

**B — 자율 탐험·동료 전투 안정화(B-1/B-2).** A의 대표 자산·Windows 기반은 재사용하며 원작 시간·일부 OS 검증은 남아 있다. C는 식사·수면·기상만 일부 구현됐고, 활동 연동·전체 지원 감사·장기 성능 검증은 미완료다. [단계 정의](docs/DEVELOPMENT_PIPELINE.md), [개발 맥락](docs/PROJECT_HISTORY.md).

## 실행·검증 기준

- 기본 탐험3층, 파이리 동료, 적/우호 모두 녹색 켈리몬. MDES7 작성·MDES1~7 읽기.
- 현재 EXE의 소스 기준은 `6ccb38c`. [기존 CI](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38717413606) core/windows 성공, Linux12·Python26·Windows13검사. [상세 증거](docs/validation/2026-10-15-companion-summary.json). 후속 요구에 대한 검증은 아니다.
- 실행: **Exit → work pull → 루트 MysteryDungeonPet.exe**. 기존 저장 삭제 불필요.

## 진행 중·남은 작업

- 문서 역할 정리 완료(`4abe078`): 과거 상태는 [이력](docs/PROJECT_HISTORY.md)과 [보관본](docs/archive/2026-10-15-project-state.md)으로 이동. README를 간단 소개로 교체.
- B 후속: 양방향 독립 전투·적극적 동료/전투 후 복귀, 포위 시 전투/대기, 직선 이동·저체력 지연, 방 중복, 동료 HP 숨김, 녹색 우호/보라색 적, 기본7층·기존3층 저장 호환.
- [작업 계획·항목별 예상 시간](docs/PARTY_AI_WORK_PLAN.md), [변경·시간 로그](docs/logs/2026-10-15-party-ai-followup.md). 새 요구는 아직 미해결이다.

## 막힘·바로 다음 작업

관리형 실행 환경이 오프라인으로 shell/build/test를 실행할 수 없다. GitHub 문서 편집·커밋은 가능하며, 로컬 HEAD·미커밋 변경은 확인하지 못했다.

2단계는 [고정 회귀·배우별 전투/저장 설계 초안](docs/PARTY_COMBAT_DESIGN.md)을 준비했다. 회귀 코드·빌드·실행은 미착수다. 다음은 고정 회귀 재현과 설계 확정이다. 실행 환경이 복구되면 fetch 후 HEAD/origin/work와 미커밋 변경을 먼저 확인하고 진행한다. 검증을 실행하기 전 해당 단계 완료를 선언하지 않는다.
