# 현재 상태

기준: 2026-10-15 KST / 개발·조회 브랜치 `work`.

## 현재 단계

**B — 자율 탐험·동료 전투 안정화(B-1/B-2).** A의 대표 자산·Windows 기반은 재사용하며 원작 시간·일부 OS 검증은 남아 있다. C는 식사·수면·기상만 일부 구현됐고, 활동 연동·전체 지원 감사·장기 성능 검증은 미완료다. [단계 정의](docs/DEVELOPMENT_PIPELINE.md), [개발 맥락](docs/PROJECT_HISTORY.md).

## 실행·검증 기준

- 기본 탐험3층, 파이리 동료, 적/우호 모두 녹색 켈리몬. MDES7 작성·MDES1~7 읽기.
- 현재 EXE 소스는 `6ccb38c`. [기존 정상 CI](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38717413606)와 [검증 증거](docs/validation/2026-10-15-companion-summary.json)를 유지한다. 후속 기능 수정·새 EXE는 아직 없다.
- 실행: **Exit → work pull → 루트 MysteryDungeonPet.exe**. 기존 저장 삭제 불필요.

## 현재 진행

- 2단계 재현 기준선: [CI38729302845](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38729302845) Linux ASan/UBSan·Windows MSVC에서 후속10검사 중 같은7개 실패·3개 통과. 실패를 고정한 결과이며 해결 완료가 아니다.
- 새로 확인한 문제: 저장 복원 여부에 따라 같은 목표·라운드의 다음 이동이 달라진다. 적 없는 HP1/HP5 보행은 모두400ms였다.
- [회귀 실행·판정](docs/PARTY_AI_REGRESSION.md), [전투·저장 설계](docs/PARTY_COMBAT_DESIGN.md), [항목별 예상 시간](docs/PARTY_AI_WORK_PLAN.md), [변경·시간 로그](docs/logs/2026-10-15-party-ai-followup.md).

## 진행 경로·바로 다음 작업

관리형 shell은 없지만 [원격 빌드·검증·실행물 갱신 경로](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38730684329)를 검증했다. 작업 브랜치 `fix/party-ai-runtime`에서3단계 배우별 전투·MDES8을 구현 중이다. [3단계 로그](docs/logs/2026-10-15-party-ai-stage3.md). 아직 새 구현의 검증은 끝나지 않았다.

다음은3단계 첫 통합 빌드와 배우별 전투/완료·MDES8/구형 저장 이행 검증이다. 이어4단계에서 직선/경로 캐시·포위/도주/복귀를 수정하고5단계 색상·HP표시·7층·방 중복,6단계 통합 검증/EXE 갱신을 진행한다. 현재 명시 회귀 `pet_companion_tests --party-ai-followup`는7실패하며 기본 CTest 성공과 구분한다.
