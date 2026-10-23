# MysteryDungeonPet

『포켓몬 불가사의 던전: 하늘의 탐험대』의 원작 데이터·그래픽·애니메이션을 재사용하는 Windows용 데스크톱 펫 프로젝트다.

작은 투명 창에서 선택한 주인공과 동료가 8개 원작 원정을 자동 탐험한다. 구간 사이에는 원작 쉼터를 거쳐 전진하고 마지막에만 귀가한다. 적과 우호 NPC는 평상시 배회하며 원작 아이템 그림과 구간 시작 맵 이름을 표시한다. 출현 종의 실제 몸체·얼굴과 상황별 문답, 나눔·길 안내·상점·공동 전투 등9종의 탐험 사건을 지원한다. 완료된 탐험3일마다 마을·온천 휴일을 번갈아 보내고, 길드 식사·취침 뒤 다음날로 이어진다. C++17/Win32/WIC 기반이며 실행 중 Python이나 에뮬레이터를 상주시킬 필요가 없다.

## 실행

실행 중인 앱을 **Exit**로 종료하고 `work` 브랜치를 pull한 뒤, 저장소 루트의 **MysteryDungeonPet.exe**를 실행한다. 일반 실행은 길드 아침에서 동료의 밝은 인사 후 함께 출발한다. 첫 원정은 8개 후보에서, 일반 재실행은 현재와 다른 후보에서 무작위로 고른다. 2줄 상태창을 표시하며 초상화는 실제 발화 중에만 나타난다. 노공룡의 기상 외침과 동료의 출발 인사를 사용한다. 휴일 방문을 마치고 식사 전에 종료했다면 남은 저녁을 먼저 잇는다. 누적 진행은 유지하고 이전 탐험의 위치·목표는 재개하지 않는다. 자세한 실행·저장·진단 방법은 [Windows 실행 안내](docs/WINDOWS_QUICKSTART.md)를 따른다.

## 개발 현황

현재는 **B 단계의 자율 탐험·동료 전투 안정화**를 진행한다. 생활 장면 일부는 구현됐으며, PC 활동 연동·전체 지원 감사·최종 성능 검증은 남아 있다. 단계 정의와 미검증 범위는 [현재 상태](PROJECT_STATE.md)에서 확인한다.

## 문서

- [플레이테스트 수정: 회복·대사·상시 상태창](docs/PLAYTEST_FEEDBACK.md)
- [현재 상태·바로 다음 작업](PROJECT_STATE.md)
- [A/B/C 개발 계획·완료 기준](docs/DEVELOPMENT_PIPELINE.md)
- [개발 맥락·주요 이력](docs/PROJECT_HISTORY.md)
- [동료 AI 후속 작업 계획](docs/PARTY_AI_WORK_PLAN.md)
- [다양성 확장용 원작 맵·출현·아이템·에셋 자료](docs/research/2026-10-20-variety-original-data.md)
- [현재 대화 대본·새 후보 32개](docs/dialogue/SPOKEN_SCRIPT.md)
- [상태·알림·아이템 이름과 반응](docs/dialogue/DISPLAY_TEXT.md)
- [오랭열매 사용 검증](docs/dialogue/ORAN_AUDIT.md)
- [이전 상황별 대사 초안·말투·감정·초상화 분류](docs/research/2026-10-20-variety-dialogue-catalogue.md)
- [탐험·휴일 이벤트 10종 명세와 원작 리소스](docs/research/2026-10-21-ten-events.md)
- [이벤트 대사 120문장·화자·감정 연결](docs/research/2026-10-21-ten-event-dialogues.md)
- [다양성·이벤트 구현 순서와 검증 계획](docs/plans/2026-10-21-event-variety-implementation.md)
- [직접 수정·빌드 안내](QUICK_EDIT_GUIDE.md)
- [설계 결정](docs/DECISIONS.md) · [작업 로그](docs/logs/)

[연속 원정 지원 범위·저장·분기](docs/CONTINUOUS_EXPEDITIONS.md)
