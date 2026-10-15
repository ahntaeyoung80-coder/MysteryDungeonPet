# 직접 수정 안내서

컷신 시간, 인사 문구, 글자 모양, 방 이탈 경로 등을 직접 바꿀 때 보는 안내서다. **GitHub Desktop에서 기본 브랜치 `work`의 최신 내용을 pull한 뒤 수정한다.** GitHub 첫 화면과 아래 소스 링크는 모두 통합된 `work`를 가리킨다.

기준: 2026-10-14, 구현 `2f2530d`. 파일 링크를 열거나 VS Code에서 `Ctrl+P`로 파일 경로를 입력하고, `Ctrl+F`로 표의 검색어를 찾는다. 줄 번호보다 함수 이름·상수 이름으로 찾으면 이후 수정으로 줄이 이동해도 같은 영역을 찾기 쉽다.

**C++ 파일의 숫자나 문구를 수정한 뒤에는 EXE를 다시 빌드해야 화면에 반영된다.** GitHub에서 파일만 편집하거나 commit/push하는 것만으로 EXE가 자동 갱신되지는 않는다. 실행 반영은 [7. 수정 적용·확인·되돌리기](#7-수정-적용확인되돌리기)를 따른다.

2026-10-15 이동·카메라 검증과 프레임 기록은 [검증 재현 안내](docs/MOVEMENT_VALIDATION.md)를 따른다. 이 안내서의 동작 수치·수정 위치는 유지된다.

## 목차

- [1. 빠른 위치 찾기](#1-빠른-위치-찾기)
  - [1.1 자주 바꾸는 항목](#11-자주-바꾸는-항목)
  - [1.2 시간·좌표·색상 읽는 법](#12-시간좌표색상-읽는-법)
- [2. 컷신 시간과 순서](#2-컷신-시간과-순서)
  - [2.1 귀가·식사·식당 카메라](#21-귀가식사식당-카메라)
  - [2.2 취침·기상·인사](#22-취침기상인사)
  - [2.3 층 전환·사망·열매 메시지](#23-층-전환사망열매-메시지)
- [3. 텍스트와 화면 표시](#3-텍스트와-화면-표시)
  - [3.1 대사·안내 문구](#31-대사안내-문구)
  - [3.2 글꼴·크기·위치·텍스트 상자](#32-글꼴크기위치텍스트-상자)
  - [3.3 하트·노란 원반·확대 배율](#33-하트노란-원반확대-배율)
- [4. 캐릭터 동작·배경·이동](#4-캐릭터-동작배경이동)
  - [4.1 동작 번호·반복·배우 완료](#41-동작-번호반복배우-완료)
  - [4.2 방 이탈 경로·방향·카메라](#42-방-이탈-경로방향카메라)
  - [4.3 낮·밤 배경·동료·길드원 배치](#43-낮밤-배경동료길드원-배치)
- [5. 탐험·전투·아이템](#5-탐험전투아이템)
  - [5.1 탐험 목표와 저체력 도주](#51-탐험-목표와-저체력-도주)
  - [5.2 HP·열매 조건·공격 확률](#52-hp열매-조건공격-확률)
  - [5.3 이동 시간·아이템 턴·NPC 만남](#53-이동-시간아이템-턴npc-만남)
- [6. 폴더와 저장 데이터](#6-폴더와-저장-데이터)
  - [6.1 소스·실행 자산·생성 도구](#61-소스실행-자산생성-도구)
  - [6.2 기존 저장과 새 실행](#62-기존-저장과-새-실행)
- [7. 수정 적용·확인·되돌리기](#7-수정-적용확인되돌리기)
  - [7.1 처음 한 번 준비](#71-처음-한-번-준비)
  - [7.2 수정 후 EXE 갱신](#72-수정-후-exe-갱신)
  - [7.3 검사·GitHub 반영·되돌리기](#73-검사github-반영되돌리기)

## 1. 빠른 위치 찾기

### 1.1 자주 바꾸는 항목

| 바꾸고 싶은 것 | 파일 | 검색어 | 지금 값 / 수정 시작점 |
| --- | --- | --- | --- |
| 식사 시간 | [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp) | `kMealMs` | `7000` = 7초 |
| 인사·열매·사망 문구 | [src/core/gameplay_scene.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay_scene.cpp) | `gameplayText` | `L"..."` 안의 문구 |
| NPC 대화 유지 시간 | [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp) | `DialogueStage::Npc`, `GameplayEvent::DialoguePlayer` | 각 단계 `1200` = 최소 1.2초 |
| 글자 크기·글꼴 | [src/windows/text_overlay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/windows/text_overlay.cpp) | `CreateFontW` | `-13`, `L"Malgun Gothic"` |
| 방을 나가는 길 | [src/core/gameplay_scene.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay_scene.cpp) | `bedroomExit` | `path`의 좌표 목록 |
| 층 전환 fade | [src/core/exploration.hpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/exploration.hpp) | `kFloorFadeMs` | 각 `1000` = 1초 |
| 노란 원반 표시 조건 | [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp) | `showFriendlyDisc` | 던전 우호 NPC만 |
| HP·도주 기준 | [src/core/exploration.hpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/exploration.hpp) | `kMaxHp`, `kLowHp` | `5`, `2` |

### 1.2 시간·좌표·색상 읽는 법

- `gameplay.cpp`의 시간은 보통 **밀리초(ms)**다. `1000`은 1초, `750`은 0.75초다. `main.cpp`의 `guildCamera_.begin(...,4000000)`과 `motion.hpp`의 이동 시간은 **마이크로초(us)**다. `4000000`은 4초다.
- 장면 좌표 `{x,y}`는 왼쪽 위가 기준이다. x를 줄이면 왼쪽, y를 줄이면 위로 이동한다. 던전의 타일 번호와 침실의 픽셀 좌표는 단위가 다르다.
- `RECT{left,top,right,bottom}`은 텍스트 영역의 네 경계다. 오른쪽·아래 값은 폭·높이가 아니라 끝 좌표다.
- `RGB(red,green,blue)`는 빨강·초록·파랑 순서다. `gameplayOverlay` 내부의 `pixel(x,y,blue,green,red)`는 파랑·초록·빨강 순서이므로 구분한다. 각 색상은 `0`~`255`다.
- `L"문구"`에서 `L`, 따옴표, 마지막 `;`를 유지하고 문구만 바꾼다. 파일은 UTF-8로 저장한다. 줄바꿈은 문자열 안에 `\n`으로 적는다.

## 2. 컷신 시간과 순서

### 2.1 귀가·식사·식당 카메라

**시간과 장면 전환:** [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp)의 `beginReturn`, `LifePhase::ReturnHome`, `LifePhase::Eat`를 찾는다.

| 항목 | 검색어 / 현재 값 | 어떻게 바꾸나 |
| --- | --- | --- |
| 공동 식사 | `kMealMs = 7000` | 원하는 ms로 변경한다. 예: `6000`이면 6초다. |
| 귀가 fade | `kFadeMs = 750`, `2 * kFadeMs` | fade-out/in 각각 0.75초다. 아래 표시용 fade와 함께 맞춘다. |
| 식사 후 밤 침실로 넘어가기 | `g.sceneBeat=SceneBeat::DiningExit` 뒤의 `750` | 이 단계의 fade 시간이다. 다른 `750`을 모두 바꾸는 방식은 피하고 해당 장면을 찾아 수정한다. |
| 식당 카메라 이동 | [src/windows/main.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/windows/main.cpp)의 `guildCamera_.begin` | `{368,212}`에서 `{168,212}`로 `4000000`us 동안 이동한다. 좌표와 마지막 시간을 바꾼다. |

식사 시간을 줄여도 캐릭터 한 명의 Eat 동작 완료로 공동 식사를 끝내지는 않는다. 짧게 바꾼 후 길드원의 동작이 충분히 보이는지 확인한다. `meal:` 키와 `g.onceKeys.insert`는 음식 소비가 한 번만 일어나게 하는 부분이다.

### 2.2 취침·기상·인사

**위치:** [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp)의 `LifePhase::Sleep`, `LifePhase::Wake`, `LifePhase::Ready`.

현재 순서는 **밤 침실 fade-in → 취침 인사 → 눕기 → 잠들기 한 번 → fade-out → 낮 침실 fade-in → 기상 → 아침 인사 → 왼쪽 통로 이탈 → 탐험**이다.

| 단계 | 검색어 | 시간·완료 조건 |
| --- | --- | --- |
| 밤 침실 표시 | `SceneBeat::NightFadeIn` | `kFadeMs` 동안 fade-in |
| 취침 인사 | `case SceneBeat::BedFace` | 플레이어와 동료의 Nod 완료 후 진행 |
| 눕기 | `case SceneBeat::BedLay` / 그 직전의 `400` | 최소 400ms와 두 배우 완료를 모두 기다림 |
| 잠들기 | `case SceneBeat::Sleeping` | 두 배우의 Sleep 한 번 완료. `5000`ms는 무한 대기 방지 마감 |
| 아침 전환 | `SceneBeat::BedFadeOut`, Wake의 `SceneBeat::BedFadeIn` | 각각 `kFadeMs` |
| 기상·아침 인사 | `LifePhase::Wake`, `SceneBeat::MorningFace` | 두 배우의 완료 신호로 진행 |
| 방 이탈 | `SceneBeat::RoomExit` | `kRoomExitMs = 1870`과 경로 이동 완료 |

**`kFiniteTimeoutMs = 5000`은 모든 동작을 5초씩 보여주는 설정이 아니다.** 인사·기상은 완료 신호가 핵심이므로 이 숫자만 늘려도 오래 머물지 않는다. 인사 문구를 일정 시간 더 읽게 하려면 해당 단계 진입의 `life(..., duration)`을 원하는 ms로 정하고, 완료 조건에 `now < g.deadlineMs` 검사도 함께 넣어야 한다. 배우 완료 조건은 유지한다. 이런 순서·완료 조건 변경은 숫자 교체보다 범위가 크다.

`kWakeDelayMs = 2000`과 `WakeDelay`는 이전 저장 호환용이다. 현재 새 취침 순서에는 2초 대기가 없으므로 이 값만 바꿔서는 새 장면의 속도가 바뀌지 않는다.

### 2.3 층 전환·사망·열매 메시지

| 항목 | 위치 / 검색어 | 함께 확인할 부분 |
| --- | --- | --- |
| 계단 fade-out/in | [src/core/exploration.hpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/exploration.hpp) / `kFloorFadeMs = 1000` | 층 전환의 논리 시간과 표시 모두 이 상수를 사용한다. |
| 사망 fade | [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp) / `GameplayEvent::CombatDefeat,750` | [gameplay_scene.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay_scene.cpp)의 `DeathStage`, `elapsed*255/750`도 맞춘다. |
| 사망 검정 문구 유지 | `DeathStage::Blackout` 직전의 `now+1000` | 검정 화면을 1초 유지한다. |
| 열매 메시지 유지 | `CombatPhase::BerryText`의 `1000`, `SceneBeat::ExplorationBerryText`의 `now+1000` | 전투 중과 탐험 중을 각각 바꾼다. |

생활 fade를 변경할 때는 **장면을 넘기는 시간**과 **검정색 농도를 계산하는 시간**을 같이 맞춘다. [gameplay_scene.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay_scene.cpp)의 `gameplayOverlay`, `const auto duration`, `LifePhase::ReturnHome`에 `750`, `1500`이 있다. [gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp)의 해당 `750`/`kFadeMs`와 불일치하면 fade 도중 장면이 튈 수 있다.

## 3. 텍스트와 화면 표시

### 3.1 대사·안내 문구

**문구는 한 곳:** [src/core/gameplay_scene.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay_scene.cpp)의 `gameplayText`.

| 검색할 문구 | 표시 장면 |
| --- | --- |
| `오랭열매! 2하트 회복` | 전투·탐험 중 열매 사용 |
| `안녕! 같이 탐험하자.` | NPC에게 플레이어가 인사 |
| `조심해서 다녀와!` | NPC의 응답 |
| `잘 자! 내일도 같이 탐험하자.` | 동료와 취침 인사 |
| `좋은 아침! 다녀올게.` | 동료와 기상 인사 |
| `눈앞이 깜깜해졌다` | 사망 후 검정 화면 |

예를 들어 `L"좋은 아침! 다녀올게."`를 `L"잘 잤어? 다녀올게!"`로 바꾸고 EXE를 갱신한다. 표시 조건인 `g.life`, `g.sceneBeat`는 그대로 둬도 된다. 긴 문장은 200px 창에서 잘릴 수 있으므로 두 줄 안에 들어오는 길이부터 시도한다. 원작 대사 추출물이 아니라 현재 런타임에 직접 적힌 문구다.

### 3.2 글꼴·크기·위치·텍스트 상자

**글자:** [src/windows/text_overlay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/windows/text_overlay.cpp)의 `TextOverlay::TextOverlay`, `TextOverlay::draw`.

| 항목 | 검색어 / 수정 예 |
| --- | --- |
| 글자 크기 | `CreateFontW(-13` → `CreateFontW(-15`로 바꾸면 더 커진다. 음수 부호를 유지한다. |
| 글꼴 | `L"Malgun Gothic"`를 설치된 한글 글꼴 이름으로 바꾼다. |
| 글자 색 | `SetTextColor(dc,RGB(255,255,255))`의 세 값 변경 |
| 일반 문구 위치 | `RECT{12,154,188,188}`의 경계 변경 |
| 검정 화면 문구 위치 | `RECT{8,80,192,130}`의 경계 변경 |
| 정렬·줄바꿈 | `DT_CENTER`, `DT_WORDBREAK`가 가운데 정렬·자동 줄바꿈을 담당 |

**상자 배경:** [src/core/gameplay_scene.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay_scene.cpp)의 `gameplayOverlay`에서 `pixel(x,y,40,30,20)`을 찾는다. 일반 상자 범위는 x=8~191, y=150~189다. 열매 상자와 인사 상자의 분기가 따로 있으므로 함께 바꾼다.

상자를 이동·확대하면 `text_overlay.cpp`의 `RECT`와 아래쪽 alpha 보정 반복문의 범위도 같은 영역으로 맞춘다. `GdiFlush()`와 alpha `255` 보정은 글자가 투명하게 사라지는 문제를 막는 부분이므로 유지한다.

### 3.3 하트·노란 원반·확대 배율

- **하트 모양·색·간격:** [src/core/gameplay_scene.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay_scene.cpp)의 `gameplayOverlay`, `static const char* heart`를 찾는다. `8 + 18*h`는 시작 위치와 하트 간격, `2*x`/`2*y`와 `yy<2`/`xx<2`는 확대, `h<g.hearts?230:65`는 채워진/빈 하트의 빨간색 농도다.
- **노란 원반 표시 조건:** [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp)의 `showFriendlyDisc`. 실제 소품 배치는 [src/windows/main.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/windows/main.cpp)의 `addDisc`다.
- **확대 배율:** 실행 중 우클릭 `Scale1..3`으로 먼저 조절한다. 기본 탐험 배율은 [main.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/windows/main.cpp)의 옵션 처리에서 `o.scale`을 찾는다. 창 자체의 `PhysicalSize = 200`은 배율과 별개이며 텍스트·UI 검사도 200px을 기준으로 한다.

## 4. 캐릭터 동작·배경·이동

### 4.1 동작 번호·반복·배우 완료

**주인공 동작 선택:** [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp)의 `gameplayClip`, `gameplayLoop`.

| 번호 | 현재 동작 |
| --- | --- |
| `0` | Walk / 이동 |
| `1` | Attack / 공격 |
| `6` | Hurt / 피격·감정 표시 |
| `7` | Idle / 대기 |
| `13` | EventSleep / 잠들기 |
| `14` | Wake / 기상 |
| `15` | Eat / 식사 |
| `17` | Pose / 클리어 포즈 |
| `22` | Nod / 인사 |
| `27` | Laying / 눕기 |
| `33` | Faint / 쓰러짐 |

해당 장면의 `return 22` 같은 숫자를 기존 연결 동작 번호로 교체한다. 실제 패키지에 없는 번호는 실행 오류가 된다. 동료는 [src/windows/main.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/windows/main.cpp)의 `advanceGuildActors`, 적·NPC는 `enemyClip_`에서 선택한다.

`gameplayLoop`는 반복 여부다. 완료 신호를 기다리는 인사·기상·잠들기를 반복으로 바꾸면 다음 장면으로 못 넘어갈 수 있다. 원본 애니메이션 자체의 프레임 시간은 패키지의 `ticks`/속도 값과 [src/core/animator.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/animator.cpp)가 담당한다. `kFiniteTimeoutMs`는 재생 속도 설정이 아니다. 눕기27은 기존 한 프레임 동작이며 숫자를 바꿔 새 전환 프레임을 만들 수는 없다.

### 4.2 방 이탈 경로·방향·카메라

**경로:** [src/core/gameplay_scene.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay_scene.cpp)의 `bedroomExit`.

```cpp
const std::array<Point,5> path{{start,{184,200},{128,200},{112,184},{104,184}}};
```

`start`는 현재 침상 좌표(204,184)이고 뒤의 점들을 차례로 지난다. 끝점(104,184)은 왼쪽 통로 입구다. 경로를 바꿀 때는 동료 침상과 벽을 피하고, 맵 안의 통로에서 끝내도록 좌표를 조절한다. 점 개수가 바뀌면 `Point,5`의 `5`도 바꾼다. 같은 좌표를 연속으로 넣으면 구간 시간이 0이 될 수 있으므로 각 점은 다르게 둔다.

이동 시간은 구간 길이 × `400/24`로 계산한다. 현재 합은 **1870ms**다. 좌표나 속도를 바꾸면 각 구간의 `lround(hypot(dx,dy)*400/24)`를 합한 값으로 [src/core/exploration.hpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/exploration.hpp)의 `kRoomExitMs`도 맞춘다. 한쪽만 바꾸면 도착 후 서 있거나 퇴실 완료가 늦어진다.

방향 번호는 `0` 아래, `1` 오른쪽 아래, `2` 오른쪽, `3` 오른쪽 위, `4` 위, `5` 왼쪽 위, `6` 왼쪽, `7` 왼쪽 아래다. 이탈 방향은 좌표 차이로 자동 계산된다. 카메라는 [main.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/windows/main.cpp)의 `SceneBeat::RoomExit`, [src/core/motion.hpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/motion.hpp)의 `cameraFor`에서 같은 경로를 따라간다.

### 4.3 낮·밤 배경·동료·길드원 배치

- **낮·밤 선택:** [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp)의 `bedroomScene`, `nightScene`. 실제 배경 선택은 [main.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/windows/main.cpp)의 `homeAssets_->package.frames`다. 현재 배경 번호는 식당0·낮 침실1·밤 침실2다.
- **동료 방향·동작:** `advanceGuildActors`에서 `GuildActorKind::Partner`와 `BedFace`, `MorningFace`를 찾는다. 주인공 방향도 `main.cpp`의 해당 장면 분기에서 정한다.
- **배우 좌표·식사 동작 원본:** 실행은 [assets/original/gameplay/guild/guild.mdgs](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/assets/original/gameplay/guild/guild.mdgs)를 읽는다. 이 파일은 바이너리다. 설명용 `provenance.json`만 편집해도 실행 좌표는 바뀌지 않는다. 생성 위치는 [tools/prepare_guild_scene.py](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/tools/prepare_guild_scene.py)의 `rooms`, `position`, `direction`, `clip`이며 읽는 코드는 [src/core/guild_scene.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/guild_scene.cpp)다.

배치 자산 변경은 생성→대조→실행 자산 교체까지 필요하다. 빠른 문구·시간 수정과 작업량이 다르므로 기존 좌표를 보려면 `guild/provenance.json`, 전체 생성 절차는 [길드 장면 문서](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/docs/GUILD_LIFE_SCENE.md)의 재현 항목을 참고한다. 그 문서의 과거 8초 식사·10초 수면 설명보다 이 안내서와 최신 코드가 우선이다.

## 5. 탐험·전투·아이템

### 5.1 탐험 목표와 저체력 도주

**위치:** [src/core/exploration.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/exploration.cpp).

| 검색어 | 역할 |
| --- | --- |
| `selectPriority` | 평상시 아이템 → frontier 후보 선택 |
| `plan` | 목표와 경로 선택, 계단·보조 목표 처리 |
| `reevaluateStairs` | 계단으로 가다가 새 아이템/frontier 발견 시 재평가 |
| `selectSurvival` | 저체력에서 먼저 발견한 열매/계단 순서로 생존 목표 선택 |
| `playerOccupancy` | 저체력에서 적 공격 범위를 피해 경로 계산 |
| `lowHealth` | `hearts <= kLowHp` 판단 |

이 영역은 숫자 하나보다 조건과 호출 순서가 결과를 결정한다. 목표 선택을 바꾸려면 해당 함수부터 읽고, `invalidate`를 아무 곳에나 추가하지 않는다. 기존 목표의 반복 취소·무한 재계획을 만들 수 있다.

### 5.2 HP·열매 조건·공격 확률

| 항목 | 위치 / 검색어 | 현재 값·수정 범위 |
| --- | --- | --- |
| 최대 HP / 도주 HP | [src/core/exploration.hpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/exploration.hpp) / `kMaxHp`, `kLowHp` | 5 / 2. 기존 저장·하트 배치·검사에도 영향을 준다. |
| 열매 사용 조건 | [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp) / `g.hearts+2 <= kMaxHp`, `g.hearts+2<=kMaxHp` | 최소 2칸 손실. 전투와 탐험 두 곳을 같이 변경한다. |
| 열매 회복량 | `CombatPhase::PlayerTurn`의 `g.hearts + 2`, `scheduleDungeon`의 `g.hearts+=2` | 두 곳의 HP 상한 처리와 3.1의 회복 문구도 맞춘다. |
| 명중 / 급소 확률 | `rollBasicAttack` | 명중 `< 80`, 플레이어 급소 `20U`, 적 급소 `5U` |
| 일반 / 급소 피해 | `result.damage = result.critical ? 2 : 1` | 1 / 2. 저장 검증의 피해 상한도 2이므로 크게 바꾸면 별도 수정 필요. |
| 우호·적 배정 비율 | `assignRelationships`, `rng.bounded(4) != 0` | 현재 우호 약 1/4, 적 약 3/4 |

HP 차감은 `EnemyTurn`의 공격 판정과 `EnemyAttack`의 `pendingHit` 제거로 나뉜다. 공격 종료 후 차감·저장 복원 중복 방지를 담당하므로 피해량을 조절할 때 이 구조를 유지한다.

### 5.3 이동 시간·아이템 턴·NPC 만남

- **한 타일 이동:** [src/core/gameplay.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/gameplay.cpp)의 `scheduleDungeon`에서 `MoveRecord`와 `now+400`; [src/core/motion.hpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/motion.hpp)의 `kMoveDurationUs = 400000`; [src/core/exploration.hpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/exploration.hpp)의 `MoveRecord::durationMs = 400`이 연결된다. [src/core/exploration.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/core/exploration.cpp)의 `validateMove`도 `durationMs==400`을 검사한다. 속도를 바꾸려면 표시·턴 마감·저장 검증·[main.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/src/windows/main.cpp)의 `restoreMovement`에 있는400도 함께 맞춰야 한다.
- **아이템 획득:** `scheduleDungeon`의 `case EventKind::Pickup`. `playerActionRound`는 한 턴 소비, `deadlineMs=now+400`은 표시 대기다. 대기만 줄여도 턴 소비는 유지할 수 있다.
- **NPC 인사·통과:** `scheduleDungeon`의 `GameplayEvent::DialoguePlayer` → `advanceGameplay`의 `LifePhase::Dialogue` → `exploration.cpp`의 `Explorer::advance` 안 `friendly(e)` 자리 교환이다. 대화 시간은 시작·응답의 두 `1200`을 바꾼다. `done`과 `greet:` 키는 인사를 반복하지 않는 기록이다.
- **배치 요청 수:** `exploration.hpp`의 `SpawnPolicy::itemRequest = 3`, `friendRequest = 2`. 적·우호 NPC가 공통 Friend 후보에서 배정되므로 `friendRequest`는 우호 NPC만의 수가 아니다. 이미 생성된 저장에는 다음 새 지도/run부터 적용된다.

## 6. 폴더와 저장 데이터

### 6.1 소스·실행 자산·생성 도구

| 위치 | 용도 |
| --- | --- |
| `src/core/` | 장면 순서·시간·문구·탐험·전투·저장 로직 |
| `src/windows/` | 실제 창·글자·배우 표시·카메라·Windows 사건 |
| `assets/original/gameplay/` | 연결된 배우·배경·길드·소품 패키지 |
| `assets/original/exploration/` | 던전 지도 프로필·타일 패키지 |
| `tools/prepare_*.py` | 자산 생성 도구. 실행 중 읽는 설정 파일과는 다름 |
| `tests/` | 동작·저장·픽셀·Windows 실행 검사 |
| `MysteryDungeonPet.exe` | 실제 실행 파일. 소스 변경 후 갱신 필요 |
| `windows-runtime.json` | EXE·소스·실행 자산 hash 기록. 갱신 도구가 생성 |

현재 문구·시간용 별도 사용자 설정 JSON은 없다. 문구는 `gameplayText`, 시간은 상수나 장면 분기에 직접 적혀 있다. `.mdpa`, `.mdgs`, `.mdpe`, `.mdes`는 바이너리이므로 메모장에서 숫자를 찾아 고치는 대상이 아니다.

### 6.2 기존 저장과 새 실행

탐색기 주소창에 `%LOCALAPPDATA%\MysteryDungeonPet`을 입력하면 저장·로그 폴더를 열 수 있다. 게임 진행은 `exploration.mdes`, 창 위치·배율은 `window.state`, 이벤트는 `events.log`, 측정은 `metrics.csv`다. 실행 파일이 있는 폴더와 저장 폴더는 다르다.

문구는 새 EXE에서 바로 읽지만, 진행 중인 장면의 기존 `deadlineMs`나 이미 생성된 NPC·지도·HP는 저장에 남을 수 있다. 식사 시간을 고쳤는데 현재 식사가 예전 시간대로 끝나면 **다음 식사**에서 확인한다. 새 run이 꼭 필요하면 실행 중인 앱을 종료하고 저장 폴더를 별도로 복사한 뒤 다음을 사용한다.

```powershell
.\MysteryDungeonPet.exe --new-run
```

이 명령은 기존 진행 대신 새 run을 저장하므로 백업을 먼저 한다. 저장의 enum 번호, `onceKeys`, `pending-hit`, CRC와 MDES schema는 문구·시간을 바꾸려고 수정할 필요가 없다. 저장 구조 변경 설명은 [MDES5 형식](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/docs/implementation/MDES5_FORMAT.md)을 참고한다.

## 7. 수정 적용·확인·되돌리기

### 7.1 처음 한 번 준비

1. GitHub Desktop에서 저장소를 열고 `work`로 전환한 뒤 pull한다. VS Code 같은 편집기로 저장소 폴더를 연다.
2. Windows에 **Visual Studio 또는 Build Tools의 「C++를 사용한 데스크톱 개발」**을 설치한다. MSVC x64 도구, Windows SDK, CMake 도구가 필요하다.
3. Python 3을 설치한다. EXE hash 기록 갱신에 사용한다. 문구·C++ 시간 변경만 할 때는 원작 자산 재추출이나 Python 자산 변환 패키지 설치가 필요 없다.
4. **Developer PowerShell for Visual Studio**를 열고 저장소 폴더로 이동한다. `cmake --version`, `python --version`이 실행되는지 확인한다. 아래 모든 명령은 저장소 루트에서 실행한다.

### 7.2 수정 후 EXE 갱신

앱을 종료하고, 예를 들어 `gameplayText`의 문구나 `kMealMs` 숫자를 수정·저장한 뒤 다음을 실행한다.

```powershell
powershell -ExecutionPolicy Bypass -File tools/update_windows_runtime.ps1
python tools/check_windows_runtime.py
.\MysteryDungeonPet.exe
```

[tools/update_windows_runtime.ps1](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/tools/update_windows_runtime.ps1)은 C++를 빌드하고, 새 EXE를 루트에 복사하고, `windows-runtime.json`을 함께 갱신한다. `runtime/source/assets hashes match`가 나오면 파일 연결이 일치한다. 이 검사는 게임 동작 자체의 통과를 뜻하지 않으므로 수정한 장면도 실행해서 확인한다.

빌드 오류가 나면 GitHub Desktop의 Changes에서 방금 바꾼 따옴표·세미콜론·괄호를 먼저 확인한다. 생성된 EXE만 고치는 것이 아니라 소스를 수정하고 다시 빌드하는 순서다. ZIP은 만들지 않는다.

### 7.3 검사·GitHub 반영·되돌리기

컷신 순서·HP·경로 같은 동작을 바꿨다면 다음 코어 검사를 실행할 수 있다.

```powershell
cmake -S . -B build/edit-check -A x64 -DBUILD_TESTING=ON
cmake --build build/edit-check --config Release
ctest --test-dir build/edit-check -C Release --output-on-failure
```

검사에는 현재 식사7초·퇴실1870ms·열매 조건·저장 복원 같은 확정 수치도 들어 있다. 의도적으로 값을 바꾸면 해당 기대값을 조정할 수 있지만, 실패 원인을 확인하고 동작을 실행해 본 뒤 바꾼다. 관련 위치는 [tests/gameplay_tests.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/tests/gameplay_tests.cpp), [tests/survival_tests.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/tests/survival_tests.cpp), [tests/gameplay_windows_tests.ps1](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/tests/gameplay_windows_tests.ps1), [tests/text_overlay_windows_tests.cpp](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/tests/text_overlay_windows_tests.cpp)다. 실제 GUI까지 검사하려면 [tools/run_windows_checks.ps1](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/blob/work/tools/run_windows_checks.ps1)을 사용한다.

GitHub Desktop에서는 **수정한 소스 + 새 `MysteryDungeonPet.exe` + 새 `windows-runtime.json` + 필요한 검사/자산 파일**을 함께 commit하고 push한다. `build/`, `local/`, 개인 저장·로그는 올리지 않는다. GitHub Actions는 검사하며, 루트 EXE를 대신 자동 생성해 commit하지 않는다.

아직 commit하지 않은 수정을 되돌리려면 Changes에서 해당 파일의 Discard changes를 사용한다. 이미 commit한 수정은 History의 Revert로 되돌린다. 소스와 EXE·hash 기록을 같은 변경 단위로 되돌리면 서로 맞지 않는 상태를 피할 수 있다. 기존 게임 진행을 되돌리는 것은 Git revert와 별개이며 백업한 저장 파일을 앱 종료 후 복원한다.
