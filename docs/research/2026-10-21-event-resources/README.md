# 이벤트 10종 구현 준비용 원작 리소스

2026-10-21 KST. [이벤트 명세](../2026-10-21-ten-events.md)와 [새 한국어 대사 초안](../2026-10-21-ten-event-dialogues.md)에 대응하는 **문서용 자료**다. runtime 자산 폴더나 패키지에는 등록하지 않았다. 원본 ROM은 읽기 전용으로 조사했으며 SHA256은 `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`다.

## 배경과 장면 근거

| 그림 | 용도 | 원작 해상도 | 원본 식별 |
| --- | --- | --- | --- |
|[P01P01A.png](P01P01A.png) | 마을 입구 길목 |432×456 |ground P01P01A |
|[T01P01A.png](T01P01A.png) |마을 서쪽 |672×504 |ground T01P01A |
|[T01P02A.png](T01P02A.png) |마을 동쪽 |552×504 |ground T01P02A |
|[P02P01A.png](P02P01A.png) |온천 |504×408 |enter00의 banner178/SetGround 및 코터스 배치로 확인 |
|[D10P41A.png](D10P41A.png) |현재 탐험 완료 장소·풍경 감상 |648×624 |기존 Steam Cave Peak ground |

이 그림은 원작 BMA/BPC/BPL/BPA의 **정적 프레임**이다. 카메라·배우·물결 효과·가림·BPA/DPLA 시간 변화가 합성된 게임 화면이 아니다. 특히 온천의 마른 탕처럼 보이는 부분은 별도 물 효과가 필요하다. [backgrounds.json](backgrounds.json)에 원본 파일 경로/해시, static level, SSA/SSE/SSS 배우 배치, SSB 파일 목록과 PNG의 RGBA 해시를 보존했다. 원작 배치 전체를 제품의 실제 경로로 곧바로 사용하지 않는다.

[script-evidence.json](script-evidence.json)은 선별 SSB 8개의 경로·해시와 ground/효과/동작/표정 명령만 담았다. 원작 메시지 본문은 미해독이므로 인용하지 않았다. `P02P01A/m05a0603.ssa`의 코터스는 actor113→MD352이며 위치 (316,132), 방향7이다. 원작 던전178의 무작위 층 자료와 휴일 ground 방문은 별개다.

## 동작·얼굴

- [actor-cues.json](actor-cues.json): 원작 3개 MONSTER pack을 병합·파싱한 40 MD의 동작 행/프레임 수. 현재 스타팅 34 MD는 모두 Float20에 8개 비어 있지 않은 방향 행을 갖는다. 현재 스타팅 runtime 패키지의 13개 clip에는 Float20이 없다. atlas 추출·제품 적용은 미실시다.
- 같은 자료의 scriptAnimationTable: overlay11 `0x023218CC`, script69→raw0x0814→WAN20/Float, script63→raw0x100E→WAN14/Wake. 스크립트 코드와 WAN clip을 같은 ID로 취급하지 않는다. Sit23/DeepBreath21은 스타팅 전원에 있지만 1방향 행이다.
- 비버니 actor124/MD434와 가재군 actor115/MD369의 기존 guild 패키지는 Idle7/EventSleep13만 갖는다. 원본에 있는 Walk/Attack/Hurt를 제품 방문용으로 추출해야 한다. 코터스352는 확인한 범위에서 기본0/1/6/7이 있으며 Float가 없어 탕 가장자리 배우로 준비한다.
- [npc-portraits.json](npc-portraits.json), [npc-moods.png](npc-moods.png): 코터스352, 켈리몬383, 비버니434, 가재군369, 예시 그랑블237/해너츠191의 원작 감정과 기본 얼굴 RGBA 해시. 그림의 열은 감정0~13이고 빈칸은 결손이다. 결손 감정은 같은 캐릭터 Normal0로 대체하는 초안을 작성했다. 다른 종의 얼굴을 빌리지 않는다.

## 효과·함정·아이템

| 자료 | 확보한 내용 | 미확인 범위 |
| --- | --- | --- |
|[effects.json](effects.json) |overlay10 `0x022CC52C`, 28바이트 레코드. 온천625~629, 느낌표88, 땀방울92의 프레임 스트립 |실제 엔진의 팔레트 인수 적용·방향·가림·벽시계 시간 |
|[effect-625.png](effect-625.png), [626](effect-626.png), [627](effect-627.png), [628](effect-628.png), [629](effect-629.png) |온천 SSB의 SetEffect와 연결. type2/effect.bin slot1, paletteArgument1, animation16~20. 각각 3프레임/36tick, loop |원작 방식으로 배우에 겹치는 높이·가림·팔레트 |
|[effect-88.png](effect-88.png), [effect-92.png](effect-92.png) |느낌표 9프레임/23tick, 땀방울 5프레임/48tick. 후보 연출 |이벤트에 적용할 대상/시점은 새 기획 |
|[props.json](props.json), [워프](prop-6.png), [구조 지점](prop-29.png), [매대](prop-30.png) |DUNGEON/dungeon.bin slot1033, traps.trp.img의 sprite6/29/30, palette1, 24×24 |실제 워프 이동·상점 거래·구조 기능 |
|[event-items.json](event-items.json) |오랭70, 사과109, 큰사과110, 돈183의 sprite/palette/원작 가격 |제품 효과·거래 비율. 돈 아이콘이 있다는 이유로 화폐 기능이 있는 것은 아님 |

효과 그림은 WAN 내부 piece palette로 변환한 비교 자료다. 원작 tick을 곧바로 밀리초로 바꾸지 않았다. **SteamClouds43**은 type3/slot33/paletteArgument14/animation2, 20프레임·80tick까지 파싱했으나 `GeneratePiece`의 palette index out of range로 PNG 변환에 실패했다. 외부 팔레트 적용 경로가 미해결이며 온천 SSB에서 사용했다고 확인한 효과도 아니다. 효과43의 PNG는 없다.

함정 팔레트1은 [고정 SkyTemple SpriteProvider](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/core/sprite_provider.py)의 `TRAP_PALETTE_MAP`과 대조했다. 그 소스도 ROM 팔레트 표 읽기를 TODO로 두고 있으므로 별도 원본 매핑표를 복원했다고 주장하지 않는다. 워프 시각 효과 ID는 미확정이며 `MOVE_WARP369`는 기술 ID다.

## 발생 조건과 검증

[map-eligibility.json](map-eligibility.json)은 앞선 1,795개 유효 층에서 상점/MH 확률 양수와 워프 함정 가중치·함정 밀도 양수인 층을 추린 것이다. 상점76/MH111/워프150은 **던전 정의 수**이며 최종 허용 맵이나 실제 원작 발생률이 아니다. 기본 던전15의 1~7F는 세 조건이 모두0이다.

[summary.json](summary.json)에 조사 기준·원본 해시·남은 확인을, [validation.json](validation.json)에 문서/자료 검증 결과를, [files.json](files.json)에 이 폴더 자료의 크기·SHA256을 둔다. 문서 검증은 실제 게임 이벤트 실행 검증을 뜻하지 않는다. 이전 전수 자료는 [원작 다양성 자료](../2026-10-20-variety-data/README.md)를 참조한다. 아이템 전체 그림과 맵별 출현표를 중복 복사하지 않았다.
