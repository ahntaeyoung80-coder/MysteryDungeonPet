# 길드 공동 식사·주인공 방 생활 장면

현재 변경(2026-10-20): 일반 실행은 길드 아침부터 시작한다. 노공룡의 원작 동작3/기상 대사/화면 흔들림2초와 푸크린의 원작 사과 아이템109 머리 합성, 두 배우 공동 퇴실을 추가했다. 원작 알람 음원·별도 효과624 전체는 남아 있다. [현재 실행](WINDOWS_QUICKSTART.md), [결정](DECISIONS.md).

아래는 2026-10-13 최초 구현 이력이다. 사용자 우선 요구. 길드 식당에서 길드원과 함께 식사하고, 길드의 주인공 방에서 취침·기상하는 장면은 필수다.

## 원본 확인과 연결

| 장면 | 원본 | 구현 |
| --- | --- | --- |
| 공동 식사 | G01P06B/c00a0601.ssa/.ssb, map G01P06B | 주인공·동료·길드원10명, 원본 자리·방향·개별 동작 |
| 식탁 음식 | SSA object107, GROUND/g01p06b1.wan | 원본 object frame·anchor·duration, 별도 depth layer |
| 주인공 방 | G01P07A/c00a0201.ssa/.ssb, map G01P07A | 주인공(204,184)/Right2, 동료(156,184)/Left6, 각자의 침상 |
| 생활 상태 | 기존 Explorer/Animator/StateStore | ReturnHome→공동 식사8초→침상 Sleep13→두 개체 Wake14→Ready→Explore |

길드원 원작 MDindex는 SSA actor 정의의 참조를 사용하며 국가 도감 번호와 혼동하지 않는다.

| 길드원 | actorId | MDindex | 식사 WAN |
| --- | --- | --- | --- |
| 노공룡 | 109 | 322 | EventSleep13 |
| 비버니 | 124 | 434 | EventSleep13 |
| 가재군 | 115 | 369 | EventSleep13 |
| 치렁 | 119 | 990 | EventSleep13 |
| 페라페 | 131 | 483 | Wake14 loop |
| 디그다 | 81 | 50 | EventSleep13 |
| 닥트리오 | 82 | 51 | EventSleep13 |
| 삐딱구리 | 133 | 495 | EventSleep13 |
| 해루미 | 101 | 792 | EventSleep13 |
| 푸크린 | 79 | 40 | Walk0, 상·하 방향 반복 |

주인공·동료는 현재 선택된 MD1/MD4 몸체와 Eat15를 쓴다. 원작 SSA의 PLAYER/ATTENDANT 참조는 플레이어의 선택 종을 런타임에서 연결하는 자리다. NPC 모두에 Eat15를 강제하지 않는다.

## 완료·미완료

- [x] 원본 식당12개 개체와 주인공 방의 침상2개 확인.
- [x] 길드원10명의 실제 원작 몸체·각각의 지정 동작과 식탁 소품 연결.
- [x] 200px 식당 viewport에서 전체 참석자 표시, 침상 좌표·방향 적용.
- [x] 공동 식사8초와 once 소비, Sleep deadline, 두 개체 기상 완료 gate.
- [x] 직접 원본 WAN→merged→atlas 대조516개 atlas/996개 원본 frame, 원본 불변.
- [x] 길드원10명 전부 frame 변화·침상 위치와 방향, 기존12개 시나리오·저장복원 검사.
- [x] Linux4CTests·Python23tests·Windows 크로스 빌드.
- [x] Windows CI38399667063 core/windows success. 실제GUI 식당12명·침실2명·동료Wake 완료→Ready→day2 Explore 확인. 귀가1522.529ms·식사8177.284ms.
- [ ] Eating642/EatingSlow643/AppleOnHead624의 별도 effect graphic·음악·대사·Turn2Direction 보간 완전 재현.
- [ ] 실제 Windows11·잠금/절전·장기 성능.

SSB 문자열의 알 수 없는 문자 오류는 opcode 추출과 분리한다. 문자 bytes를 보존해 명령만 분석했으며 대사의 정상 디코딩으로 보고하지 않는다. 원본 Laying27 지정은 기록했고 수면 loop는 기존 승인된EventSleep13을 사용한다. 전체 원작 story VM을 구현한 것은 아니다.

## 재현과 미리보기

```sh
python tools/prepare_gameplay_assets.py --out local/gameplay-guild-new
cmake -S . -B build/b1 -DCMAKE_BUILD_TYPE=Release
cmake --build build/b1
build/b1/pet_gameplay_tests assets/original/gameplay
python tools/preview_guild_scene.py --assets assets/original/gameplay --inspector build/b1/pet_package_inspect --out local/guild-preview
```

`assets/original/gameplay/guild/provenance.json`은 원본 script/SSA/MD/WAN hash와 각 개체의 원작 자리·script code·effect ID·검사 결과를 보존한다. `guild.mdgs`는 native에서 범위·경로·중복을 검사해 읽는다. 미리보기는 실제 MDPA frame/anchor/tick을 native inspector에서 읽어 합성한 GIF다. Windows 실제 실행과 별도로 검증한다.
