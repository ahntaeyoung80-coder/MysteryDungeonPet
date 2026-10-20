# MysteryDungeonPet

『포켓몬 불가사의 던전: 하늘의 탐험대』의 원작 데이터·그래픽·애니메이션을 재사용하는 Windows용 데스크톱 펫 프로젝트다.

작은 투명 창에서 주인공과 파이리 동료가 기본7층 던전을 자동 탐험하고 동료가 가까운 전투를 보조한다. 녹색 켈리몬은 우호 NPC, 보라색 켈리몬은 적으로 등장한다. 탐험을 마치면 길드에서 함께 식사하고, 잠든 뒤 다음날 다시 탐험한다. C++17/Win32/WIC 기반이며 실행 중 Python이나 에뮬레이터를 상주시킬 필요가 없다.

## 실행

실행 중인 앱을 **Exit**로 종료하고 `work` 브랜치를 pull한 뒤, 저장소 루트의 **MysteryDungeonPet.exe**를 실행한다. 실행할 때마다 길드의 아침에서 시작해 노공룡 알람과 기상 인사 후 동료와 함께 출발한다. 누적 진행은 유지하고 이전 탐험의 위치·목표는 재개하지 않는다. 자세한 실행·저장·진단 방법은 [Windows 실행 안내](docs/WINDOWS_QUICKSTART.md)를 따른다.

## 개발 현황

현재는 **B 단계의 자율 탐험·동료 전투 안정화**를 진행한다. 생활 장면 일부는 구현됐으며, PC 활동 연동·전체 지원 감사·최종 성능 검증은 남아 있다. 단계 정의와 미검증 범위는 [현재 상태](PROJECT_STATE.md)에서 확인한다.

## 문서

- [현재 상태·바로 다음 작업](PROJECT_STATE.md)
- [A/B/C 개발 계획·완료 기준](docs/DEVELOPMENT_PIPELINE.md)
- [개발 맥락·주요 이력](docs/PROJECT_HISTORY.md)
- [동료 AI 후속 작업 계획](docs/PARTY_AI_WORK_PLAN.md)
- [직접 수정·빌드 안내](QUICK_EDIT_GUIDE.md)
- [설계 결정](docs/DECISIONS.md) · [작업 로그](docs/logs/)
