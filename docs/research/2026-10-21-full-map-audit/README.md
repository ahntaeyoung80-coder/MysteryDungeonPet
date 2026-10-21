# 원작 탐험 맵 전체 대조

스토리상 연속 진행과 중간 쉼터는 후속 [연속 원정·캥카 연결부 분류](../2026-10-21-continuous-expeditions/README.md)를 참고한다. 이 문서의 MAPPA 계열과 원정 단위는 서로 다르다.

사용자 요청 기준일은 2026-10-21이다. 실제 추출·조회 시각은 [summary.json](summary.json)의 UTC를 따른다. 원작 ROM을 새로 읽고 웹의 전체 장소 목록 및 91개 고유 개별 문서를 직접 대조했다. 일부 후보만 추린 앞선 답변을 대체한다. 제품 코드·실행 자산·EXE·출현 설정은 변경하지 않았다.

## 확인한 전체 범위와 집계 단위

**일반 탐험·우회로 69계열, 특별 에피소드 18계열, 데모 6계열, 도장 12미로가 확인된다.** 일반 69계열은 본편 주경로 23 + 우회로 3 + 후일담 주경로 11 + 선택 탐험 32로 나뉜다. 같은 장소의 상층·깊은 곳·정상/보스방은 하나의 계열로 묶고, 특별 에피소드의 동명 장소는 별도 판본으로 구분했다. 합계 105개는 이 기준으로 나눈 루트/미로 항목이며 게임 화면의 공식 독립 던전 총수로 주장하지 않는다.

| 단위 | 확인 수 | 의미 |
| --- | ---: | --- |
| 일반 탐험·우회로 계열 |69|현재 두 구간 외에도 다수의 미연결 원작 장소 존재 |
| 특별 에피소드 계열 |18|같은 이름이라도 본편과 다른 group/타일셋/출현 데이터 |
| 데모 계열 |6|원작 기본 설정에서는Dummy이지만 디버그 enum·웹에서 이름 식별 |
| 도장 미로 |12|타입별10개×5층, Explorer1층, Final48층 |
| named MAPPA group |94|69+18+6+도장1군. 나머지6군은테스트/구조/용도미확정 슬롯 |
| MAPPA 물리 그룹/층 |100 / 1,892|원본mappa_s 전체; 모든 층 구조·출현/아이템/함정 문맥 파싱 |
| 일반 ARM9 정의 |180|보스·중간방·테스트·시스템/이름용 항목도 포함; 실제 장소 수와 다름 |
| 일반 정의의 요청/유효 논리 층 |1,801 / 1,795|범위 오류6개는 임의 대체하지 않고 별도 보존 |
| 도장 추가 층 |99|일반180 정의 밖의 특수 인덱싱을 이번에 포함 |
| restriction/name ID 공간 |256|256개 실제 탐험 맵이라는 뜻이 아님 |
| ROM FAT 파일 / FNT 이름 파일 |8,644 / 8,608|전 파일 크기·SHA256 목록.36개는FNT 이름 없는 파일 |
| 실제 제품 패키지 / 기본 풀 |0·15·18 / 15·18|연결한 범위가 작았던 것이며 다른 맵의 원본이 없는 상태가 아님 |

물리 층 1,892개는 모두 파싱됐으며 오류는 없었다. mappa_gs의 타일셋·고정층 보조표와도 전부 일치했다. 이전에 추출한 논리 층 1,795개의 layout·normal/MH 비중을 다시 대조한 결과도 불일치가 없었다. ROM의 SHA256은 조사 전후 일치했다. 정규 타일셋 125개는 원작 청크·팔레트 이미지 변환과 DMA 참조까지 확인했다. 고정층에서만 사용하는 170 이상 타일 ID 26개는 일반 타일셋 변환 범위와 분리했다.

## 왜 제품에는 두 맵만 있었나

원작 자료가 부족해서가 아니다. 기존 개발에서 **Steam Cave 전체 중 Upper Steam Cave(ID 15)의 7층, Amp Plains 전체 중 Far Amp Plains(ID 18)의 9층만** 실행 패키지·NPC/아이템 그림·완료 장소에 연결했다. 두 장소의 앞 구간인 ID 14(8층)와 ID 17(10층)조차 기본 풀에 없었다. 완료 ground까지 검증한 연결만 사용하던 구현 범위를 원작 후보 전체처럼 좁혀 설명한 것이 잘못이었다.

기존에 ‘해안 동굴’이라고 표시한 패키지 0은 원작의 `Test Dungeon`이다. **실제 Beach Cave는 ID 1의 일반 4층 + ID 2의 고정 1층**이다. 원본 설정 이름과 제품 표시 이름을 구분한다. 이름표 자체는 이번 조회 작업에서 변경하지 않았다.

맵을 제품에 추가하려면 전체 구간·타일셋, NPC 몸체와 얼굴, 아이템 그림과 표시명, 완료 지점, 다음 날 선택, 저장 복원, 실제 EXE를 연결해야 한다. 이번에 확인한 범위는 데이터와 정적 그림이다. 원작 전체의 이야기·지형 생성·보스·날씨·지형 효과를 제품에서 실행 검증한 결과는 아니다.

## 일반 탐험·우회로 69계열 전체

ROM 층 합계는 각 구간 정의의 논리 층을 더한 값이다. 비고정/고정 구분은 MAPPA의 fixed_floor_id를 기준으로 한다. 웹 문서가 보스방·목표방을 포함하는 기준과 다를 수 있으며, 분기와 중간방까지 합친 값이 한 번의 실제 탐험 길이와 같지는 않다. G는 MAPPA group, ID는 dungeon ID다.

| 분류 | G | 장소 | 원작 구간 ID | ROM 유효층(비고정+고정) | 현재 제품 |
| --- | ---: | --- | --- | ---: | --- |
|본편 주경로|1|[Beach Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Beach_Cave)|1,2|5 (4+1)|미연결|
|본편 주경로|2|[Drenched Bluff](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Drenched_Bluff)|3|6 (6+0)|미연결|
|본편 주경로|3|[Mt. Bristle](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Mt._Bristle)|4,5|10 (9+1)|미연결|
|본편 주경로|4|[Waterfall Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Waterfall_Cave)|6|8 (8+0)|미연결|
|본편 주경로|5|[Apple Woods](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Apple_Woods)|7|12 (12+0)|미연결|
|본편 주경로|6|[Craggy Coast](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Craggy_Coast)|8|9 (9+0)|미연결|
|본편 우회로|7|[Side Path](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Side_Path)|9|5 (5+0)|미연결|
|본편 주경로|8|[Mt. Horn](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Mt._Horn)|10|14 (14+0)|미연결|
|본편 우회로|9|[Rock Path](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Rock_Path)|11|5 (5+0)|미연결|
|본편 주경로|10|[Foggy Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Foggy_Forest)|12|11 (11+0)|미연결|
|본편 우회로|11|[Forest Path](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Forest_Path)|13|5 (5+0)|미연결|
|본편 주경로|12|[Steam Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Steam_Cave)|14,15,16|16 (15+1)|부분 연결: 15|
|본편 주경로|13|[Amp Plains](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Amp_Plains)|17,18,19|20 (19+1)|부분 연결: 18|
|본편 주경로|14|[Northern Desert](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Northern_Desert)|20|15 (15+0)|미연결|
|본편 주경로|15|[Quicksand Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Quicksand_Cave)|21,22,23|21 (20+1)|미연결|
|본편 주경로|16|[Crystal Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Crystal_Cave)|24|11 (11+0)|미연결|
|본편 주경로|17|[Crystal Crossing](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Crystal_Crossing)|25,26|14 (13+1)|미연결|
|본편 주경로|18|[Chasm Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Chasm_Cave)|27|8 (8+0)|미연결|
|본편 주경로|19|[Dark Hill](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Dark_Hill)|28|15 (15+0)|미연결|
|본편 주경로|20|[Sealed Ruin](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Sealed_Ruin)|29,30,31|15 (14+1)|미연결|
|본편 주경로|21|[Dusk Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Dusk_Forest)|32|8 (8+0)|미연결|
|본편 주경로|22|[Deep Dusk Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Deep_Dusk_Forest)|33|12 (12+0)|미연결|
|본편 주경로|23|[Treeshroud Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Treeshroud_Forest)|34|20 (20+0)|미연결|
|본편 주경로|24|[Brine Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Brine_Cave)|35,36,37|15 (14+1)|미연결|
|본편 주경로|25|[Hidden Land](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Hidden_Land)|38,39,40|24 (23+1)|미연결|
|본편 주경로|26|[Temporal Tower](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Temporal_Tower)|41,42,43|24 (23+1)|미연결|
|후일담 주경로|27|[Mystifying Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Mystifying_Forest)|44,45|14 (13+1)|미연결|
|선택 탐험|28|[Midnight Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Midnight_Forest)|98|25 (23+2)|미연결|
|선택 탐험|29|[Lake Afar](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Lake_Afar)|93|25 (23+2)|미연결|
|선택 탐험|30|[Mt. Mistral](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Mt._Mistral)|95|20 (18+2)|미연결|
|선택 탐험|31|[Shimmer Hill](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Shimmer_Hill)|96|18 (16+2)|미연결|
|선택 탐험|32|[Bottomless Sea](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Bottomless_Sea)|73,74|50 (49+1)|미연결|
|후일담 주경로|33|[Surrounded Sea](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Surrounded_Sea)|50|20 (19+1)|미연결|
|후일담 주경로|34|[Miracle Sea](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Miracle_Sea)|51,52,53|23 (21+2)|미연결|
|선택 탐험|35|[Concealed Ruins](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Concealed_Ruins)|70,71|30 (28+2); 미해결1|미연결|
|선택 탐험|36|[Lost Wilderness](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Lost_Wilderness)|97|18 (16+2)|미연결|
|선택 탐험|37|[Mystery Jungle](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Mystery_Jungle)|85,86|30 (29+1)|미연결|
|선택 탐험|38|[Happy Outlook](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Happy_Outlook)|94|20 (18+2)|미연결|
|선택 탐험|39|[Serenity River](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Serenity_River)|87|8 (7+1)|미연결|
|선택 탐험|40|[Landslide Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Landslide_Cave)|88|13 (12+1)|미연결|
|후일담 주경로|41|[Blizzard Island](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Blizzard_Island)|46|20 (20+0)|미연결|
|선택 탐험|42|[Shimmer Desert](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Shimmer_Desert)|75,76|10 (9+1)|미연결|
|후일담 주경로|43|[Crevice Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Crevice_Cave)|47,48,49|15 (14+1)|미연결|
|선택 탐험|44|[Mt. Avalanche](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Mt._Avalanche)|77,78|20 (19+1)|미연결|
|선택 탐험|45|[Giant Volcano](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Giant_Volcano)|79,80|20 (19+1)|미연결|
|선택 탐험|46|[World Abyss](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:World_Abyss)|81,82|30 (29+1)|미연결|
|선택 탐험|47|[Lush Prairie](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Lush_Prairie)|89|6 (5+1)|미연결|
|선택 탐험|48|[Tiny Meadow](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Tiny_Meadow)|90|5 (4+1)|미연결|
|선택 탐험|49|[Labyrinth Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Labyrinth_Cave)|91|10 (9+1)|미연결|
|선택 탐험|50|[Oran Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Oran_Forest)|92|5 (4+1)|미연결|
|선택 탐험|51|[Sky Stairway](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Sky_Stairway)|83,84|50 (49+1)|미연결|
|후일담 주경로|52|[Aegis Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Aegis_Cave)|54,55,56,57,58,59,60,61|19 (15+4)|미연결|
|후일담 주경로|54|[Dark Crater](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Dark_Crater)|67,68,69|30 (29+1)|미연결|
|선택 탐험|55|[Marine Resort](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Marine_Resort)|72|19 (18+1)|미연결|
|후일담 주경로|56|[Mt. Travail](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Mt._Travail)|62|19 (19+0)|미연결|
|후일담 주경로|57|[The Nightmare](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:The_Nightmare)|63|17 (17+0)|미연결|
|후일담 주경로|58|[Spacial Rift](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Spacial_Rift)|64,65,66|25 (23+2)|미연결|
|선택 탐험|59|[Zero Isle North](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Zero_Isle_North)|99|75 (64+11)|미연결|
|선택 탐험|60|[Zero Isle West](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Zero_Isle_West)|101|40 (31+9)|미연결|
|선택 탐험|61|[Zero Isle East](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Zero_Isle_East)|100|40 (35+5)|미연결|
|선택 탐험|62|[Zero Isle South](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Zero_Isle_South)|102|99 (95+4)|미연결|
|선택 탐험|64|[Zero Isle Center](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Zero_Isle_Center)|103|50 (49+1)|미연결|
|선택 탐험|65|[Destiny Tower](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Destiny_Tower)|104|99 (99+0)|미연결|
|선택 탐험|68|[Oblivion Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Oblivion_Forest)|107|15 (14+1)|미연결|
|선택 탐험|69|[Treacherous Waters](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Treacherous_Waters)|108|20 (19+1)|미연결|
|선택 탐험|70|[Southeastern Islands](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Southeastern_Islands)|109|25 (24+1)|미연결|
|선택 탐험|71|[Inferno Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Inferno_Cave)|110|30 (29+1)|미연결|
|후일담 주경로|72|[Sky Peak Mountain Path](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Sky_Peak_Mountain_Path)|111,112,113,114,115,116,117,118,119,120,121,122|40 (38+2)|미연결|
|선택 탐험|91|[Star Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Star_Cave)|174|16 (16+0)|미연결|

## 특별 에피소드18계열 전체

| G | 장소 | 원작 구간 ID | ROM 유효층(비고정+고정) | 주의 |
| ---: | --- | --- | ---: | --- |
|73|[Star Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Star_Cave)|123,124,125,126,127|17 (15+2)|동명 본편과별도group|
|74|[Murky Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Murky_Forest)|128|7 (7+0)|제품 미연결|
|75|[Eastern Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Eastern_Cave)|129|10 (10+0)|제품 미연결|
|76|[Fortune Ravine](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Fortune_Ravine)|130,131,132|15 (14+1)|제품 미연결|
|77|[Barren Valley](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Barren_Valley)|133,134,135|11 (10+1)|제품 미연결|
|78|[Dark Wasteland](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Dark_Wasteland)|136|4 (4+0)|제품 미연결|
|79|[Temporal Tower](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Temporal_Tower)|137,138|20 (20+0)|동명 본편과별도group|
|80|[Dusk Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Dusk_Forest)|139,140|8 (7+1)|동명 본편과별도group|
|81|[Spacial Cliffs](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Spacial_Cliffs)|141|10 (10+0)|제품 미연결|
|82|[Dark Ice Mountain](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Dark_Ice_Mountain)|142,143,144|15 (14+1)|제품 미연결|
|83|[Icicle Forest](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Icicle_Forest)|145|10 (10+0)|제품 미연결|
|84|[Vast Ice Mountain](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Vast_Ice_Mountain)|146,147,148|18 (17+1)|제품 미연결|
|85|[Southern Jungle](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Southern_Jungle)|149|10 (10+0)|제품 미연결|
|86|[Boulder Quarry](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Boulder_Quarry)|150,151,152|11 (10+1)|제품 미연결|
|87|[Right Cave Path](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Right_Cave_Path)|153|5 (5+0)|제품 미연결|
|88|[Left Cave Path](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Left_Cave_Path)|154|5 (5+0)|제품 미연결|
|89|[Limestone Cavern](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Limestone_Cavern)|155,156,157|11 (10+1)|제품 미연결|
|90|[Spring Cave](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:Spring_Cave)|158,159,160,161,162,163,164|19 (16+3)|제품 미연결|

## 데모6계열 전체

| G | 장소 | 원작 구간 ID | ROM 유효층(비고정+고정) | 주의 |
| ---: | --- | --- | ---: | --- |
|92|[Little Plains](https://bulbapedia.bulbagarden.net/wiki/Little_Plains)|165|5 (5+0)|원작 설정의Dummy표기를enum/웹으로식별|
|93|[Mt. Clear](https://bulbapedia.bulbagarden.net/wiki/Mt._Clear)|166|6 (6+0)|원작 설정의Dummy표기를enum/웹으로식별|
|94|[Challenge River](https://bulbapedia.bulbagarden.net/wiki/Challenge_River)|167|7 (7+0)|원작 설정의Dummy표기를enum/웹으로식별|
|95|[Trial Forest](https://bulbapedia.bulbagarden.net/wiki/Trial_Forest)|168|6 (6+0)|원작 설정의Dummy표기를enum/웹으로식별|
|96|[Guiding Sea](https://bulbapedia.bulbagarden.net/wiki/Guiding_Sea)|169|5 (5+0)|원작 설정의Dummy표기를enum/웹으로식별|
|97|[Hidden Shopkeeper Village](https://bulbapedia.bulbagarden.net/wiki/Hidden_Shopkeeper_Village)|170|5 (5+0)|원작 설정의Dummy표기를enum/웹으로식별|

## 도장 12미로 전체

도장은 일반 ARM9 정의 표 외의 별도 처리다. [고정SkyTemple의get_dungeon_entry/get_number_floors](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/module.py)와 [Marowak Dojo](https://bulbapedia.bulbagarden.net/wiki/Marowak_Dojo)를 대조했다. 그룹 53의 0~49는 타입별 5층씩, 50은 Explorer 1층, 51~98은 Final 48층이다.

| ID | 원작 명칭 | 층수 | MAPPA group53 범위 |
| ---: | --- | ---: | --- |
|180|Normal/Fly Maze|5|0~4|
|181|Dark/Fire Maze|5|5~9|
|182|Rock/Water Maze|5|10~14|
|183|Grass Maze|5|15~19|
|184|Elec/Steel Maze|5|20~24|
|185|Ice/Ground Maze|5|25~29|
|186|Fight/Psych Maze|5|30~34|
|187|Poison/Bug Maze|5|35~39|
|188|Dragon Maze|5|40~44|
|189|Ghost Maze|5|45~49|
|190|Explorer Maze|1|50~50|
|191|Final Maze|48|51~98|

## 구간·더미·별칭을 잘못 세지 않기

- ID 0은 Test Dungeon이다. ID 175~178의 이름은 Shaymin Village / Armaldo’s Shelter / Luminous Spring / Hot Spring이지만, ARM9 정의에서는 group 79의 층을 재참조한다. 이를 추가 탐험 맵 4개로 자동 편입하면 안 된다. 실제 거점·온천 ground는 별도 MAP_BG·SCRIPT 경로를 사용한다.
- ID 105/106, 171~173은 용도가 확정되지 않은 Dummy 정의다. 층 자료가 있어도 웹에서 확인한 정식 플레이 루트로 분류하지 않았다. **ID 165~170은 이들과 달리 데모 6종으로 식별됐다.**
- ID 71은 group 35의 존재하지 않는 30번 층, ID 173은 group 99의 존재하지 않는 5~9번 층을 가리킨다. 다른 정상 층을 복사해 오류를 숨기지 않았다.
- ID 192~211의 도장 계열 예약·별칭과 ID 212~255의 구조·합류 출처·기타 메타 ID는 별도 물리 층 정의로 세지 않았다. [256ID전체표](all-256-identities.csv), [실제일반/도장192항목](DUNGEON_ID_INDEX.md)을보존한다.
- 일반·도장 정의가 직접 참조하지 않는 물리 층 13개(group 52의 19번, group 78의 4~15번)도 확인했다. 미참조만으로 미사용을 확정하지 않았다.

## 웹과 원본의 차이

[94계열 전체 대조 CSV](web-rom-comparison.csv)에 웹 header와 ROM 전체·비고정·고정 층을 함께 기록했다. Steam Cave 15층과 Amp Plains 19층은 별도 정상방 1층을 뺀 값이며 ROM 정의 합계는 각각 16·20층이다. Miracle Sea 22층과 Spacial Rift 24층도 별도 고정 1층 구간을 제외한 범위와 맞는다. 반면 Beach Cave의 웹 표기 5층은 보스방을 포함한 4+1이다. 이를 강제로 같은 기준으로 고치지 않았다.

Drenched Bluff는 ROM에서 6층인데 MDFW header는 7이며, Bulbapedia도 header 6 / 본문 7이 혼재한다. 이견을 남기고 원본 층수 6을 보존했다. Star Cave / Temporal Tower / Dusk Forest는 본편과 특별 에피소드가 동명 문서에 섞일 수 있어 header만으로 SE 층수를 보정하지 않았다. 마스터 목록의 총수와 범주 합도 기준이 불명확하므로 그 숫자를 그대로 복사하지 않고 집계 단위를 명시했다.

전체 웹 목록과 고유 개별 문서 91개를 조회했다. 최초 timeout 3개는 재조회했고, 데모 6개의 MDFW 개별 namespace 404는 Bulbapedia 동명 문서로 보충했다. shell의 직접 HTTP는 환경 proxy의 403으로 막혀 웹 전용 도구를 사용했다. [첫시도](web-facts.json), [재조회/최종근거](web-reconciled.json).

## 그림과 재현 자료

- [93계열 원작타일 표본](all-named-route-samples.png): 도장을 제외한 일반·SE·데모의 정적 청크 표본이다. 생성한 탐험 지도 스크린샷은 아니다.
- [정규타일셋125개전체](all-regular-tilesets.png), [픽셀hash/DMA변환기록](tile-validation.json).
- [ROM파일8,644개 manifest](rom-files.csv), [MAPPA물리1,892층](physical-floors.csv), [정의/참조](dungeon-definitions.json), [요약](summary.json).
- 재현:저장소루트에서 `PYTHONPATH=. local/p1-venv/bin/python docs/research/2026-10-21-full-map-audit/audit_rom.py`, `preview_tiles.py`, `write_report.py`를같은폴더경로로실행한다. 웹facts는조회시점의추출메타데이터를사용한다.

원작 이름은 고정 디버그 enum / SkyTemple 정적 설정과 웹 명칭을 사용했다. 패치 ROM의 한국어 문자열을 새로 해독한 결과는 아니다. 원본 그림의 권리는 Nintendo / Pokémon / CHUNSOFT에 있다. 문서·데이터·그림은 제품용 asset 폴더에 추가하지 않았다.

## 주요 출처

- [MDFW 전체 장소 목록](https://mysterydungeonwiki.com/wiki/Explorers_of_Sky:List_of_Locations)
- [Bulbapedia 위치 분류](https://bulbapedia.bulbagarden.net/wiki/Category:Pok%C3%A9mon_Mystery_Dungeon:_Explorers_of_Sky_locations)
- [Serebii Sky 던전 안내](https://www.serebii.net/dungeonsky/dungeon/):웹페이지의안내번호를ROM dungeon ID와혼동하지않는다.
- [고정 pmdsky-debug ID/group 정의](https://github.com/UsernameFodder/pmdsky-debug/blob/c72fe675919ec83cfe93f687973afae85e00d1a3/headers/types/common/enums.h)
- [고정SkyTemple 일반/도장처리](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/module.py)
