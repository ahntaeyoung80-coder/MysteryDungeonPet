# 다양성 확장 원작 조사 데이터

2026-10-20 KST, EoS_NA/C2SE, 기준 커밋 `8ca52153c0cf2d4f0f6e17259793c92601162d62`. 제품 패키지가 아니라 문서용 원작 조사 결과다. 설명은 [원작 데이터 문서](../2026-10-20-variety-original-data.md)와 [대사 분류 문서](../2026-10-20-variety-dialogue-catalogue.md)를 따른다.

- `MAP_INDEX.md`: 사람이 읽는180개 맵 목록. 한국어 원문을 복원한 이름이 아니라 정적 설정/디버그 enum 이름이다.
- `maps.json`, `map-encounters.csv`: 맵별 후보·문맥·설정 범위와 MD별normal/MH 출현 층. `normalFloors/monsterHouseFloors`는 구간 안의1기반 층수이며 raw localFloor와 구분한다.
- `floor-profiles.jsonl.gz`: 유효1,795개 논리 층을 한 줄씩 저장했다. localFloor는0기반, groupFloor는startAfter+localFloor다. `normal/monsterHouse.weight`는CDF 차분/GCD로 정규화한 상대 비중이다. special rows는 별도다. item category 비중과 그 내부 item 비중도 별도다. trapWeights는 원본 trap 가중치 필드다.
- `runtime-map-comparison.json`: 현재0/15 패키지의 출현/아이템/그룹 참조 대조 결과.
- `body-availability.json`:521 MD의 실제3pack WAN 해제·병합 후 동작별 행/프레임 수. 출력 그림/픽셀 검증 완료를 뜻하지 않는다.
- `items.csv`, `item-sprites.png`:1,400개 아이템 레코드와217개sprite/palette 조합의 실제 원본RGBA hash, 그림 표본. 동일 그림을 공유하는 아이템도 논리 ID는 다르다.
- `portrait-availability.csv`, `original-portrait-moods.png`:MD별 직접 KAO행·공유 후보·원작14감정. 공유 후보와 현재 검증된 runtime clip을 구분한다. 이미지의`?`도 후보 표시다.
- `dialogue-source-index.json`:69개 원작말투 그룹×6유형, 참조1,284개와 문자열 hash. 본문은 문자표 미해독으로 포함하지 않는다. `monsterPersonalities`의 인덱스는실제MD 슬롯이며 도감 번호가 아니다.
- `script-actors.csv`:1,884개 SSA/SSE/SSS에서 읽은7,039개 actor 배치. 우호/적대 관계나던전 종료장소를 자동 확정하지 않는다.
- `script-face-sequences.json`: 선별3개SSB의 얼굴/메시지 명령 순서. 메시지 본문은 제외했다.
- `audit-summary.json`:원본파일 hash, 파싱 건수·오류·미확인 범위.
- `files.json`: 이 폴더의 자료별 byte size/SHA256. 자기 자신은 포함하지 않는다.

일반 정의71/173의6개 층은MAPPA 범위를 벗어나 원본을 대체하지 않고 오류에 남겼다. 도장180~191은 특수 인덱싱으로 미조사다. 모든 일반 정의가 실제 플레이용이라는 뜻은 아니다.

압축된 층 자료는 표준 gzip으로 읽을 수 있다.

```python
import gzip, json
with gzip.open('floor-profiles.jsonl.gz', 'rt', encoding='utf-8') as source:
    for line in source:
        floor = json.loads(line)
        if floor['dungeonId'] == 15 and floor['localFloor'] == 0:
            print(floor['normal'])
```

원본 데이터의 바이트는 보관 ROM에서 읽었다. 파일 경로·해시·파서 고정 커밋은 주 문서와 감사 요약에 있다. 영문 포켓몬 이름은 고정 pmdsky-debug enum에서 붙인 메타데이터이며 원작 문자열의 해독 결과가 아니다.
