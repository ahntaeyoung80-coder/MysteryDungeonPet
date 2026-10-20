# 이벤트 10종 대사 초안과 초상화 연결

2026-10-21 KST. [이벤트 명세·원작 리소스](2026-10-21-ten-events.md)에 대응하는 **새 한국어 작성 초안**이다. 원작 인용·해독·공식 번역이 아니며 runtime에 등록하지 않았다. 기존 고정 대사나 필수 성공 문구를 변경하지 않는다. [기계 판독 자료](2026-10-21-event-dialogue-data.json)에 문장 ID·진행 단계·화자·조건·요청 감정·실제 표정을 함께 보존했다.

## 작성·출력 기준

- Player/Partner는 선택된 실제 캐릭터를 뜻한다. EventNpc는 그 사건의 실제 NPC instance다. 예시MD는 자료 대조용이며 모든 맵에 같은 종을 고정 배치하라는 뜻이 아니다.
- 표정은 `요청→실제`로 적었다. 일반 NPC는 원작에 없는 표정을 **같은 캐릭터의 Normal0**로 대체한다. 코터스352는0/12, 켈리몬383은0/3/5, 가재군369는0/4/5, 비버니434는0/1/2/5/8/9/12/13이 원작에서 확인됐다. 예시그랑블237/해너츠191은Normal만 있다. 기존 켈리몬 보완 그림이 있어도 다른 NPC의 표정으로 대신 쓰지 않는다.
- Player/Partner의 선택 가능한 21종은 현재 14감정을 쓸 수 있다. 화자가 없는 System에는 초상화를 붙이지 않는다. 한 번에 발화자의 초상화만 표시하고 명찰은 붙이지 않는다.
- 대화 전에 실제 배우가 서로 바라보게 한다. E05의 풍경 감상은 무언 구간으로 나누고 발화 때 서로 본다. E08의 분리 중에는System 안내만 쓰며 왕복 대화는 인접 재회 후 시작한다.
- 거래/전달/반환/전투/동행 완료를 말하는 줄은 해당 결과commit 뒤에만 출력한다. HP/보상·구매·날씨·원작 대사 본문을 추측해 넣지 않는다. E04/E07 패배시 성공대사를 건너뛰고 기존101/102 기절 대사/귀가에 맡긴다.
- 문답 단위의bundle을 같은 변형으로 선택한다. 진행 중 다시 뽑지 않는다. 결과 분기는 실제결과가 확정된 시점에 선택·저장하고, 발생 초기에 미래의 성공을 확정하지 않는다. 숫자runtime ID로의배정·확률/가중치/쿨다운은 아직 미정이다.
- 현재 NPC 읽기 1,200ms/동료 1,600ms를 무조건 재사용하지 않는다. 타이핑 완료와 별도 읽기 시간을 확보하고 200×200 원작 대화창의 한글 줄바꿈을 검증해야 한다.

감정 ID:0기본,1기쁨,2아픔,3화남,4걱정,5슬픔,6울음,7외침,8울먹임,9결의,10큰기쁨,11감탄,12놀람,13어지러움. 세부 분류는 [공통 대사 문서](2026-10-20-variety-dialogue-catalogue.md)를 따른다.


## E01 지친 탐험가

### E01.share_a

조건: 안전한 조우이며 마지막 회복 예비량을 남기고 나눌 오랭열매를 예약한 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E01.share_a.01|intro|EventNpc (예시MD237)|4→0|휴... 조금 지쳤어.|
|E01.share_a.02|intro|Player|4→4|괜찮아? 이 열매를 나눠 줄게.|
|E01.share_a.03|intro|Partner|1→1|천천히 쉬면서 먹어.|
|E01.share_a.04|success|EventNpc (예시MD237)|1→0|고마워. 덕분에 마음이 놓여.|
|E01.share_a.05|success|Player|1→1|무리하지 말고 다녀와.|
|E01.share_a.06|success|EventNpc (예시MD237)|0→0|너희도 조심해서 가!|

### E01.encourage

조건: 안전하게 나눌 여분이 없어 전달량이 0인 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E01.encourage.01|intro|EventNpc (예시MD237)|4→0|잠깐 숨을 돌려야겠어.|
|E01.encourage.02|intro|Player|4→4|지금은 물건을 나누기 어렵겠어.|
|E01.encourage.03|intro|Partner|0→0|서두르지 말고 쉬어 가.|
|E01.encourage.04|success|EventNpc (예시MD237)|1→0|말만으로도 고마워. 조심해서 가.|

### E01.share_b

조건: 여분 오랭열매를 예약한 경우의 다른 말투.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E01.share_b.01|intro|Player|4→4|많이 지쳐 보여. 잠깐 쉬는 게 어때?|
|E01.share_b.02|intro|EventNpc (예시MD237)|0→0|응... 조금만 쉬려고.|
|E01.share_b.03|intro|Partner|1→1|이 열매도 챙겨 둬.|
|E01.share_b.04|success|EventNpc (예시MD237)|1→0|정말 고마워. 소중히 쓸게.|

## E02 길 잃은 포켓몬

### E02.escort_a

조건: NPC가 동행에 동의했고 같은 층 계단까지 경로가 있는 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E02.escort_a.01|intro|EventNpc (예시MD191)|4→0|저기... 계단으로 가는 길을 모르겠어.|
|E02.escort_a.02|intro|Player|1→1|우리와 같이 가자.|
|E02.escort_a.03|intro|Partner|9→9|천천히 따라오면 돼.|
|E02.escort_a.04|success|EventNpc (예시MD191)|1→0|여기까지 데려다줘서 고마워!|
|E02.escort_a.05|success|Player|1→1|이제 조심해서 가.|
|E02.escort_a.06|success|Partner|1→1|무사히 다녀와!|

### E02.escort_b

조건: 동행 대사의 다른 말투이며 실제 경로와 도착 확인이 필요한 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E02.escort_b.01|intro|EventNpc (예시MD191)|4→0|길을 잘못 든 것 같아.|
|E02.escort_b.02|intro|Player|9→9|계단까지는 함께 가 줄게.|
|E02.escort_b.03|success|EventNpc (예시MD191)|1→0|이제 길을 알겠어. 고마워!|
|E02.escort_b.04|success|Partner|1→1|도움이 돼서 다행이야.|

## E03 켈리몬의 작은 매대

### E03.trade_a

조건: 매대가 열려 있고 줄 물건과 받을 물건이 구체적으로 예약된 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E03.trade_a.01|intro|EventNpc (예시MD383)|0→0|어서 와! 물건을 보고 가.|
|E03.trade_a.02|intro|Player|0→0|교환할 수 있는 물건이 있을까?|
|E03.trade_a.03|intro|EventNpc (예시MD383)|0→0|서로 필요한 물건을 살펴보자.|
|E03.trade_a.04|success|EventNpc (예시MD383)|0→0|교환은 끝났어. 잘 챙겨 둬!|
|E03.trade_a.05|success|Player|1→1|고마워. 잘 쓸게.|
|E03.trade_a.06|success|EventNpc (예시MD383)|0→0|조심해서 다녀와!|

### E03.browse

조건: 성립하는 교환이 없어 비용 차감과 물품 수령 없이 구경하는 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E03.browse.01|intro|EventNpc (예시MD383)|0→0|구경만 하고 가도 좋아.|
|E03.browse.02|intro|Player|0→0|오늘은 어떤 물건이 있나 볼게.|
|E03.browse.03|success|Partner|1→1|다음에 또 들르자.|
|E03.browse.04|success|EventNpc (예시MD383)|0→0|언제든 환영이야!|

### E03.trade_b

조건: 구체적인 교환이 실제 성립한 경우의 다른 말투.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E03.trade_b.01|intro|EventNpc (예시MD383)|0→0|마침 교환할 물건이 있네.|
|E03.trade_b.02|intro|Player|1→1|좋아. 그걸로 바꾸자.|
|E03.trade_b.03|success|EventNpc (예시MD383)|0→0|여기 있어. 물건은 확인했지?|
|E03.trade_b.04|success|Partner|1→1|응, 잘 챙겼어!|

## E04 소규모 몬스터 하우스

### E04.house_a

조건: 사건과 적수가 확정됐고 체력과 배치 공간이 충분한 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E04.house_a.01|intro|System|없음|몬스터 하우스다!|
|E04.house_a.02|intro|Player|12→12|조심해! 둘러싸이지 말자.|
|E04.house_a.03|intro|Partner|9→9|알겠어. 함께 버티자!|
|E04.house_a.04|victory|Player|1→1|휴... 이제 잠잠해졌네.|
|E04.house_a.05|victory|Partner|9→9|주변을 살피고 움직이자.|
|E04.house_a.06|victory|Player|0→0|좋아. 천천히 가자.|

### E04.house_b

조건: 경고와 실제 승리 후 대사의 다른 말투.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E04.house_b.01|intro|Player|12→12|갑자기 나타났어!|
|E04.house_b.02|intro|Partner|9→9|침착하게 대응하자!|
|E04.house_b.03|victory|Partner|1→1|잘 버텼어!|
|E04.house_b.04|victory|Player|4→4|아직 방심하지 말자.|

## E05 탐험 중 작은 감탄

### E05.view_a

조건: 안전한 감상 장소이며 확인하지 않은 날씨나 지형을 주장하지 않는 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E05.view_a.01|view|Player|0→0|여긴 잠깐 멈춰도 좋겠다.|
|E05.view_a.02|view|Partner|1→1|응. 주변도 천천히 보자.|
|E05.view_a.03|depart|Player|11→11|함께 와서 더 기억에 남을 것 같아.|
|E05.view_a.04|depart|Partner|1→1|나도 그래. 이제 가 볼까?|

### E05.view_b

조건: 조용한 감상 대사의 다른 말투.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E05.view_b.01|view|Partner|11→11|이런 곳도 있었구나.|
|E05.view_b.02|view|Player|1→1|서두르기만 하면 놓쳤겠지.|
|E05.view_b.03|depart|Partner|0→0|오늘 본 건 기억해 둘게.|
|E05.view_b.04|depart|Player|1→1|좋아. 다시 함께 가자.|

## E06 분실물 돌려주기

### E06.return_a

조건: 분실물을 확보했고 정확한 주인 개체가 확인된 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E06.return_a.01|pickup|System|없음|누군가 잃어버린 물건을 발견했다.|
|E06.return_a.02|pickup|Player|4→4|주인이 근처에 있을까?|
|E06.return_a.03|owner|EventNpc (예시MD237)|12→0|그거, 내가 잃어버린 물건이야!|
|E06.return_a.04|owner|Player|1→1|찾고 있었구나. 돌려줄게.|
|E06.return_a.05|success|EventNpc (예시MD237)|1→0|정말 고마워. 한참 찾고 있었어.|
|E06.return_a.06|success|Partner|1→1|주인을 찾아서 다행이야.|

### E06.return_b

조건: 답례를 가정하지 않는 반환 대사의 다른 말투.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E06.return_b.01|pickup|Player|0→0|누가 떨어뜨린 모양이야.|
|E06.return_b.02|pickup|Partner|4→4|주인을 찾으면 돌려주자.|
|E06.return_b.03|owner|EventNpc (예시MD237)|4→0|혹시 내 물건을 못 봤어?|
|E06.return_b.04|owner|Player|1→1|이 물건이 맞지? 여기 있어.|
|E06.return_b.05|success|EventNpc (예시MD237)|1→0|맞아! 찾아줘서 고마워.|
|E06.return_b.06|success|Player|1→1|다음에는 잘 챙겨 둬.|

## E07 다른 탐험대와 공동 전투

### E07.assist_a

조건: 임시 아군과 적의 역할을 고정했고 안전하게 전투를 지원하는 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E07.assist_a.01|intro|EventNpc (예시MD237)|4→0|혼자서는 버거워... 도와줘!|
|E07.assist_a.02|intro|Player|9→9|함께 상대하자!|
|E07.assist_a.03|intro|Partner|9→9|옆에서 도울게!|
|E07.assist_a.04|victory|EventNpc (예시MD237)|1→0|덕분에 버텼어. 정말 고마워.|
|E07.assist_a.05|victory|Player|1→1|무사해서 다행이야.|
|E07.assist_a.06|victory|Partner|0→0|이제 각자 조심해서 가자.|

### E07.assist_b

조건: 임시 아군 지원 대사의 다른 말투.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E07.assist_b.01|intro|EventNpc (예시MD237)|4→0|잠깐만 힘을 빌려줄래?|
|E07.assist_b.02|intro|Player|9→9|좋아. 힘을 합치자.|
|E07.assist_b.03|intro|Partner|4→4|서로 떨어지지 말자!|
|E07.assist_b.04|victory|EventNpc (예시MD237)|1→0|혼자가 아니라서 든든했어.|
|E07.assist_b.05|victory|Partner|1→1|우리도 도움이 돼서 기뻐.|
|E07.assist_b.06|victory|EventNpc (예시MD237)|0→0|너희의 탐험도 잘 풀리길 바라!|

## E08 워프 함정으로 잠깐 떨어지기

### E08.reunion_a

조건: 확정된 워프 한 번 뒤 같은 층에서 재회하는 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E08.reunion_a.01|separated|System|없음|워프 함정을 밟았다!|
|E08.reunion_a.02|reunion|Player|4→4|찾았다! 괜찮아?|
|E08.reunion_a.03|reunion|Partner|1→1|응. 다시 만나서 다행이야.|
|E08.reunion_a.04|reunion|Player|4→4|다음엔 발밑도 더 살피자.|
|E08.reunion_a.05|reunion|Partner|9→9|좋아. 함께 가자.|

### E08.reunion_b

조건: 분리 중 왕복 대화 없이 재회한 뒤 나누는 다른 말투.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E08.reunion_b.01|separated|System|없음|워프 함정을 밟았다!|
|E08.reunion_b.02|reunion|Partner|4→4|휴, 어디로 갔나 했어.|
|E08.reunion_b.03|reunion|Player|1→1|나도 찾고 있었어. 무사해서 다행이야.|
|E08.reunion_b.04|reunion|Partner|0→0|이제 다시 길을 찾아보자.|
|E08.reunion_b.05|reunion|Player|9→9|응. 천천히 함께 가자.|

## E09 길드원의 깜짝 방문

### E09.bidoof

조건: 방문자가 원작 길드 비버니 actor124/MD434인 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E09.bidoof.01|intro|EventNpc (예시MD434)|1→1|어라? 여기서 다 만나네!|
|E09.bidoof.02|intro|Player|1→1|탐험하러 나온 거야?|
|E09.bidoof.03|intro|EventNpc (예시MD434)|0→0|응! 서로 무리하지 말고 다녀오자.|
|E09.bidoof.04|depart|Partner|1→1|좋아. 길드에서 또 보자!|

### E09.corphish

조건: 방문자가 원작 길드 가재군 actor115/MD369인 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E09.corphish.01|intro|EventNpc (예시MD369)|0→0|여기까지 왔구나! 잘 지내고 있지?|
|E09.corphish.02|intro|Player|1→1|응. 함께 돌아다니는 중이야.|
|E09.corphish.03|intro|EventNpc (예시MD369)|4→4|너무 무리하진 마. 길드에서 보자.|
|E09.corphish.04|depart|Partner|1→1|알겠어. 너도 조심해서 가!|

## E10 휴일: 마을 또는 온천 방문

### E10.town_a

조건: 하루 일정이 마을 휴일이며 던전 탐험이나 거래를 전제하지 않는 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E10.town_a.01|morning|Player|1→1|오늘은 탐험을 쉬고 마을에 가 볼까?|
|E10.town_a.02|morning|Partner|1→1|좋아! 천천히 둘러보자.|
|E10.town_a.03|visit|EventNpc (예시MD383)|0→0|반가워. 오늘은 쉬는 날이구나.|
|E10.town_a.04|visit|Player|0→0|응. 잠깐 산책하러 왔어.|
|E10.town_a.05|depart|Partner|1→1|서두르지 않으니 한결 편안해.|
|E10.town_a.06|depart|Player|1→1|이제 돌아가서 푹 쉬자.|

### E10.town_b

조건: 구매나 보상이 없는 마을 휴일 산책의 다른 말투.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E10.town_b.01|morning|Partner|1→1|오늘은 마을에서 쉬어 가는 거지?|
|E10.town_b.02|morning|Player|1→1|응. 가끔은 이런 날도 좋겠지.|
|E10.town_b.03|visit|EventNpc (예시MD383)|0→0|느긋하게 보고 가. 언제든 환영이야.|
|E10.town_b.04|depart|Partner|1→1|좋은 휴일이었어. 이제 돌아가자.|

### E10.spring_a

조건: 하루 일정이 온천 휴일이며 입욕 동작과 물결 장면을 사용할 수 있는 경우.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E10.spring_a.01|morning|Player|1→1|오늘은 온천에서 쉬어 가자.|
|E10.spring_a.02|morning|Partner|1→1|좋아. 느긋하게 보내고 싶어.|
|E10.spring_a.03|visit|EventNpc (예시MD352)|0→0|어서 오게. 서두를 것 없네.|
|E10.spring_a.04|bath|Partner|1→1|따뜻해서 몸이 풀리는 것 같아.|
|E10.spring_a.05|bath|Player|1→1|응. 오늘은 천천히 쉬자.|
|E10.spring_a.06|depart|EventNpc (예시MD352)|0→0|잘 쉬었나? 돌아가는 길도 조심하게.|

### E10.spring_b

조건: 최대 HP 회복을 주장하지 않는 온천 휴일의 다른 말투.

| 문장 ID | 단계 | 화자 | 감정 요청→표시 | 대사 초안 |
| --- | --- | --- | --- | --- |
|E10.spring_b.01|visit|EventNpc (예시MD352)|0→0|편히 쉬다 가게. 좋은 날이구먼.|
|E10.spring_b.02|visit|Player|1→1|오늘은 푹 쉬려고 왔어.|
|E10.spring_b.03|bath|Partner|0→0|가만히 있으니 마음도 차분해지네.|
|E10.spring_b.04|bath|Player|1→1|함께 와서 더 좋은 것 같아.|
|E10.spring_b.05|depart|Partner|1→1|이제 돌아갈까? 잘 쉬었어.|
|E10.spring_b.06|depart|Player|1→1|응. 내일은 또 내일의 하루를 보내자.|

## 검토 상태

총 10개 이벤트, 24개 대화 묶음, 120개 문장 초안이다. E10의 town/spring은 같은 휴일 사건의 목적지 변형이다. E01의 나눔/격려와 E03의 거래/구경은 실제 재고·결과 조건으로 구분하며 후보를 무작위로 섞지 않는다.

JSON의 `stageGates`는 해당 단계의 모든 문장에 적용되는 공통 조건이며 `conditions`는 여기에 추가되는 줄별 조건이다. 성공·승리·재회 조건은 첫 문장뿐 아니라 그 단계 전체를 막는다. 샘플 NPC의 원작 얼굴 존재와 표정 대체를 자료와 대조했다. 새 runtime 초상화 clip·실제 대사 로더·자동 이벤트 선택·저장/복원은 아직 미구현이다. 원작 김 효과43의 팔레트와 워프 시각 효과 ID 등 미해결 항목은 이벤트 명세에 따로 남겼다.
