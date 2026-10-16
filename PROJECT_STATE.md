# 현재 상태

기준: 2026-10-16 KST, 개발·실행 기준 브랜치 `work`.

## 현재 단계

**B-1/B-2 — 자율 탐험·동료 전투 안정화.** 독립 배우 전투·도주/복귀·이동·표시·7층/구형 저장 이행을 구현·검증했다. A의 대표 자산/Windows 기반과 C의 식사·수면·기상 일부를 재사용한다. A 잔여 원작 시간/실제 OS 검증, C 활동 연동·전체 지원 감사·24시간/최종 성능은 남아 있다. [단계 정의](docs/DEVELOPMENT_PIPELINE.md), [맥락](docs/PROJECT_HISTORY.md).

## 실행·검증 기준

- 기본7층, 파이리 동료, 녹색 우호/보라색 적, 동료 HP 내부 유지·화면 숨김. 독립 전투와 합법 복귀·400ms 공동 이동을 사용한다.
- MDES8 쓰기/MDES1~8 읽기. 기존3층 저장은 현재 탐험을 유지하고 다음날7층으로 전환한다. 명시 `--explore`는 지정 패키지를 따른다. 저장 삭제는 필요하지 않다.
- 같은 run 원시 지형과 최근64개 평행이동 전체 지형 중복을 거부하고 다음날 기억을 이어간다. 개별 방 크기 유사성·사용자 미확보 장면까지 해결한 것으로 확대하지 않는다.
- 소스 `9dd3927`·검증 실행물 `55e9006`. [통합 CI](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38830612402): Windows native15/15·후속27/27·GUI/설치/루트, Linux ASan/UBSan14/14·Python26/26 성공. [실행 안내](docs/WINDOWS_QUICKSTART.md).

## 진행·바로 다음 작업

6단계에서 `work` 반영 후 기본 CI를 확인 중이다. [변경·실측 시간](docs/logs/2026-10-16-party-ai-stage6.md), [작업 계획](docs/PARTY_AI_WORK_PLAN.md).

방 중복 사용자 장면의 정확한 유형/저장은 미확보다. 다음은 그 장면과 통제된 지형 회귀를 대조하고 B 잔여 인수 기준을 정리한 뒤 C 활동 연동 범위를 구체화하는 일이다. 실제 Windows11 잔여 항목·원작 wall time·장기 검증은 별도 gate다. 관리형 shell 없이 GitHub Actions로 검증했으며 로컬 미커밋 상태는 조회하지 못했다.
