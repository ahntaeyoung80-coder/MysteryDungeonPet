# MysteryDungeonPet

『포켓몬 불가사의 던전: 하늘의 탐험대』의 데이터와 리소스를 재활용하는 Windows용 초경량 데스크톱 펫 프로젝트.
원작 동작·출현 분포를 우선하며, 던전은 시작방·이동방·계단방의 2~3개 방으로 단순화한다.

A-1 대표 원작 데이터·자산 준비를 완료했고, A-2 Windows 시험 런타임과 공유 패키지 소비를 구현했다. 사용자 Windows11 기록을 재사용해 A-3 계측·200px 고정과 실제 원작 package의 native 통합을 검증 중이다. 추가 수동 테스트 없이 기존 결과를 재사용하고 미검증 항목은 유지한다. 작업 재개 시 [PROJECT_STATE.md](PROJECT_STATE.md)를 먼저 읽는다.

최신 사용자 지시로 A를 재사용한 B-1 비전투 탐험 코어·저장/복원과 원작 tileset0/12 변환을 구현했다. Python geometry와 native10,000 seed 결과를 별도로 보존하며 A-3 잔여 항목과 B-1의 미연결 원작 그림·Windows11 탐험 검증은 pending이다.

- [Codex 개발 파이프라인·1차 실행 명세](docs/DEVELOPMENT_PIPELINE.md)
- [B-1 비전투 탐험·원작 타일·저장/복원](docs/B1_USAGE.md)
- [A-3 착수·자동 검사·원작 연결 재개](docs/A3_USAGE.md)
- [A-3 Windows 원작 패키지 실행](docs/A3_WINDOWS_QUICKSTART.md)
- [A-2 사용자 실행 점검·A-3 재개](docs/logs/2026-10-13-a2-user-review.md)
- [A-2 빌드·패키지 연결](docs/A2_USAGE.md)
- [Windows 11 빠른 실행](docs/A2_WINDOWS_QUICKSTART.md)
- [전체 원작 입력·A-3 실행 패키지](assets/original/README.md)
- [A-1 추출·패키징 실행](docs/A1_USAGE.md)
- [구현 인수인계](docs/research/IMPLEMENTATION_HANDOFF.md)
- [조사 상태](docs/research/00_RESEARCH_STATE.md)
- [설계 결정](docs/DECISIONS.md)
- [조사 검사 자료](docs/research/checks/README.md)
- [이전 명세 원본](docs/archive/README.md)
