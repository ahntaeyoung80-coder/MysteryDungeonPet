# Phase 4 — 경량 던전 및 자율 탐험

작성 기준: 2026-10-13 KST. 원작 출현표·타일을 재사용하면서 지도는 방 2~3개와 단순 통로로 새로 생성한다. 전투·몬스터 하우스·원작 복잡한 생성기는 런타임에 도입하지 않는다. 기존 후보의 실제 코드를 확인한 뒤 최소 신규 구현을 선정했다.

## 1. 후보 비교와 결정

| 후보 | 실제 코드·입출력 | 재사용 가능 부분 | 의존성·라이선스·비용 | 판단 |
| --- | --- | --- | --- | --- |
| dungeon-mystery | [src/dungeon.ts](https://github.com/EpicYoshiMaster/dungeon-mystery/blob/a23726d8fb0038668bfb1f3119eb3db6ecf57d98/src/dungeon.ts) / GenerateDungeon(floor_props,dungeon_data,optional constants/settings/callback/frequency) → Tile[][] | 방·통로·계단 도달 검사 개념과 원작 생성 알고리즘의 참고 기준 | TS, 내부 전역 상태/RNG/원작 구조체; GPL-3.0-or-later. 런타임 크기·CPU 실측 없음 | 전체 이식 미채택; 일부 함수도 독립성이 낮아 직접 복사보다 참고 |
| pmd-bot | [agent/SimpleAgent.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/agent/SimpleAgent.lua) / Agent:act(state,visible),setTargetPos; [agent/Agent.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/agent/Agent.lua) / setTarget,findTargetPath,act | 아이템→계단/탐색 frontier 목표, 경로 캐시·복구 정책 | Lua/DeSmuME/FCEUX 호환 입력·RAM 접근과 modified Jumper. MIT; 실행 실측 없음 | 정책만 참고하고 에뮬레이터 동작은 이식 안 함 |
| modified Jumper | [utils/pathfinder.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/utils/pathfinder.lua) / getPath | A* 경로 찾기·대각 corner-cut 방지 | git submodule 의존; Lua 바인딩 또는 포팅 비용 | 24×24 균일 비용 지도에서는 미채택 |
| 신규 네이티브 | flat byte grid + FIFO BFS + 경로 캐시 | 2~3방, 연결 통로, 다른 방의 계단, 아이템/우호적 개체 | C++ 표준 라이브러리 또는 고정 배열. 추가 라이브러리 없음 | 최종 채택. 연구용 geometry 실험만 실행했으며 제품 코드 없음 |

기존 TS 엔진의 설치 용량 target과 실제 빌드 결과를 구분한다. package.json의 size-limit 10KB는 설정된 목표이며 실제 측정치를 확보한 것은 아니다.

## 2. dungeon-mystery의 실제 함수 경계

src/dungeon.ts는 조사 스냅샷에서 약 4,915줄/175,532바이트이며 생성·원작 상태·spawn이 묶여 있다. 다음 함수는 단독 공개 API가 아니라 대부분 내부 함수다. 필요한 함수 하나를 그대로 복사하면 관련 전역과 구조체까지 따라오는지 먼저 확인해야 한다.

| 기능 | 실제 함수 | 입력 → 출력 / 연결 | 적용 방법 |
| --- | --- | --- | --- |
| 방 지정 | AssignRooms,CreateRoomsAndAnchors | GridCell[][],grid size,구획 목록,RoomFlags → 전역 dungeonData 지도 변경 | 방 슬롯·중심 개념만 참고. 원작 room flags는 생략 |
| 통로 | CreateHallway(start_x,start_y,end_x,end_y,vertical,turn_x,turn_y) | 좌표 → 전역 타일 변경 | L자 1회 꺾임으로 단순화. 초기 축만 seed 기반 랜덤 |
| 연결 보완 | EnsureConnectedGrid | 구획/방 연결 → 전역 상태 보완 | 새 generator는 중심을 체인 연결하고 BFS로 검사하므로 별도 원작 복구 알고리즘 불필요 |
| 표준/2방 생성 | GenerateStandardFloor,GenerateTwoRoomsWithMonsterHouseFloor | FloorProperties/원작 설정 → 지도 | 후자는 monster house 결합 구조여서 요구한 2방 생성기로 바로 사용하지 않음 |
| 계단 | SpawnStairs(x,y,hidden_stairs_type) | 지정 타일 → 계단 spawn 정보 변경 | 숨겨진 계단 제외, 출발과 다른 방에서 BFS 거리 최대인 중심 선택 |
| 아이템 등 | SpawnNonEnemies | floor properties → spawn flags | 종류·원작 item table 선택은 별도. flags를 species/item ID로 착각하지 않음 |
| 적 출현·후처리 | SpawnEnemies,ResolveInvalidSpawns | 원작 status → spawn 변경 | 전투 제거 방침으로 미채택. 우호적 개체 배치는 별도 |
| 계단 도달 검사 | StairsAlwaysReachable(x_stairs,y_stairs,mark_unreachable) → bool | 원작 지도 → 도달 여부 | 전 그리드를 반복하는 흐름 대신 작은 큐 BFS 사용 |

[src/key_types.ts](https://github.com/EpicYoshiMaster/dungeon-mystery/blob/a23726d8fb0038668bfb1f3119eb3db6ecf57d98/src/key_types.ts)의 Tile,GridCell,FloorProperties,Dungeon,DungeonGenerationInfo와 [src/minor_types.ts](https://github.com/EpicYoshiMaster/dungeon-mystery/blob/a23726d8fb0038668bfb1f3119eb3db6ecf57d98/src/minor_types.ts)의 flags를 같이 확인한다. 원작 전체 구조체를 runtime save format으로 쓰지 않는다.

[src/random.ts](https://github.com/EpicYoshiMaster/dungeon-mystery/blob/a23726d8fb0038668bfb1f3119eb3db6ecf57d98/src/random.ts)의 RandInt/RandRange는 실제 Math.random을 호출한다. InitSeed 필드가 있다고 원작 RNG 재현·deterministic 생성이라고 주장하면 안 된다. 새 generator는 상태가 저장되는 RNG를 명시적으로 받는다. 검증용 LCG는 재현 편의를 위한 것이며 최종 RNG 선택은 신규 구현 영역이다.

## 3. pmd-bot에서 참고할 정확한 로직

| 기능 | 파일·함수 | 입력 → 출력 | 참고·제거 범위 |
| --- | --- | --- | --- |
| 단순 목표 선택 | [agent/SimpleAgent.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/agent/SimpleAgent.lua) / Agent:act,setTargetPos | state,visible/targetPos,targetName → 행동·목표/경로 갱신 | 보이는 아이템·계단 목표와 path reuse 참고. HP/PP·공격 분기 제거 |
| 탐색 정책 | [agent/Agent.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/agent/Agent.lua) / setTarget,findTargetPath,act | 미탐색/아이템/계단/soft goal → 목표·경로 | frontier 선택과 도달 실패 시 대안 정책 참고. 제품의 생활 우선순위와 별도 연결 |
| 지도 변환 | [utils/pathfinder.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/utils/pathfinder.lua) / layoutToMap(내부),getPath | layout,start/end,walkable/avoid → 경로 목록 | 원본 nil/terrain 판정 참고. 실제 getPath는 modified Jumper ASTAR 호출과 avoid-map 복사 비용이 있음 |
| 이동 변환 | 같은 파일 / getMoves,getDirection,comparePositions,stepDistance | 경로/좌표차 → 방향·비교·거리 | 함수 의미 참고. 방향 값은 원작 SSA/SSB/atlas row와 명시 변환 |
| frontier BFS | 같은 파일 / exploreLayout | layout,start,validTile,walkable → 탐색 경로 | FIFO/parent 탐색과 대각 이동 때 양쪽 corner 검사 참고. 4방향 이동이면 더 단순 |
| 원작 RAM 지도 | [dynamicinfo/mapHelpers.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/dynamicinfo/mapHelpers.lua) | 고정 주소·20byte tile·54×30 지도 → 정보 | 메모리 주소/버전 결합으로 신규 런타임에 사용하지 않음 |
| 아이템·음식·계단 조작 | [actions/smartactions.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/actions/smartactions.lua) / pickUpItemIfPossible,eatFoodIfHungry; [actions/basicactions.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/actions/basicactions.lua) | emulator state → joypad/menu 행동 | 정책 참고만. 인벤토리·생활 이벤트에 직접 연결하고 joypad/메뉴 VM은 제거 |

Jumper 서브모듈은 johanngan/Jumper의 수정판 commit 0deadbdb9aa0a7151de580ce941629e8cd7d10c0를 참조한다. 표준 upstream Jumper를 바꾸어 넣어도 동일한 corner/avoid 동작이 보장되는 것은 아니다. 최종 안에서는 이 의존성을 넣지 않는다.

## 4. 신규 지도 구조와 출현표 적용

제안 WorldGrid는 width,height,terrain[width*height],roomId[],start,stairs,version을 갖는다. terrain은 벽/바닥/물 타입을 명확히 정규화하고 Phase 1의 DMA 지형 타입으로 변환한다. 논리 좌표는 원본 24px 던전 그림 단위 및 창 배율과 별도로 관리한다.

생성 순서: 제한된 슬롯에서 2~3개 방 선택 → 각 방 사각형 floor → 중심끼리 L통로 연결 → 4방향 BFS 연결성 검사 → 시작=첫 방 중심 → 계단=다른 방 중심 중 BFS 거리 최대 → 유효 바닥에 원본 분포로 아이템·친구 배치. 계단/시작/소품 점유 타일에 중복 배치하지 않는다.

Phase 1 catalogue의 dungeonId/localFloor를 키로 가져와 normal monster relativeWeight와 floor_items의 category/item conditionalWeight를 사용한다. guaranteed 아이템은 별도 배치 정책으로 처리한다. 특별 케클레온·dummy·불명 항목을 일반 친구로 넣지 않는다. 도장/그룹 층 인덱스 규칙은 이 단계에서 재계산하지 않고 추출된 정규화 floor key를 사용한다.

원본 종류와 상대 분포는 유지하되 작은 지도에서의 배치 수·재등장 간격은 신규 정책이다. 원본 density를 그대로 24×24 지도에 적용하면 지나치게 빽빽해질 수 있으므로 spawn count 상한을 따로 명시한다. 원작 시간당 출현률을 재현했다고 표기하지 않는다.

### 배치 종료 보장

SpawnSampler는 시작·계단·소품·기존 점유를 제외하고 도달 가능한 배치 후보 타일 목록을 먼저 만든다. 요청 수는 정책 예산과 후보 수 이하로 제한하며, 타일을 선택하면 후보에서 제거한다. 랜덤 좌표 재추첨 루프는 사용하지 않는다. 후보가 비었거나 유효한 분포가 없으면 종료한다.

guaranteed 아이템은 별도 배열을 유지하고 정해진 우선순위로 예약한다. 공간을 초과한 요청은 미배치 항목과 사유를 기록하며, 무한 재시도하거나 원본 가중치를 변경하지 않는다. 고정 10% 제한은 강제하지 않고 작은 지도용 예산을 별도 정책으로 명시한다.

필수 검사: 후보 0개, 모든 타일 점유, 요청 수가 후보 수를 초과하는 경우, guaranteed 초과, 아이템·친구 간 중복 없음, 동일 시드 재현. 각 호출은 후보와 요청의 유한 순회 안에 종료해야 한다.

화면 투영은 [05_WINDOWS_RUNTIME.md](05_WINDOWS_RUNTIME.md)의 좌표·표시 영역 계약을 사용한다. 24×24 논리 맵 크기와 DIB 크기를 독립적으로 유지한다.

## 5. 탐험·생활 연결

| 상황 | 목표·전환 | 정확히 한 번 수행할 일 | 복구 |
| --- | --- | --- | --- |
| 새 층 | 가까운 미탐색 frontier 또는 아이템 | gridVersion 증가, visited 초기화 | 계단까지 연결 실패면 제한 횟수 재생성 |
| 아이템 보임 | 도달 가능한 item 우선 | instanceId로 인벤토리 추가·지도 제거 | 막힌 item은 현재 version에서 unreachable 캐시 |
| 친구 만남 | 잠깐 서로 바라보기·Nod/Hop | cooldown/event onceKey | 상대가 길 막으면 양보/짧은 대기/재계획 |
| frontier 없음/체류 한도 | stairs 목표 | 도달 시 floor increment | 계단 대체 탐색이나 안전 귀가 |
| 마지막층 | Pose 이벤트 | dungeon completed 한 번 | finite 완료 timeout 후 귀가 |
| 음식/수면 필요 | ReturnHome | 탐험 snapshot 저장 | 생활 이벤트가 이동을 잠시 중단 |

모든 포켓몬은 우호적이다. 포켓몬 간 공격·체력·기술·피해·전투 목표는 두 후보 코드에서 가져오지 않는다. 목표의 도달 여부를 확인한 뒤 점수를 계산한다. 매 프레임 전체 BFS를 실행하지 않고 goal/gridVersion/다음 이동 타일의 점유 상태가 변했을 때만 경로를 갱신한다.

4방향/동일 비용 이동이면 FIFO BFS가 충분하다. 8방향을 동일한 원작 턴 비용으로 취급할 경우에도 BFS를 쓸 수 있으나 두 인접 직교 타일의 corner 규칙을 명시한다. 대각에 sqrt(2) 비용을 부여하면 FIFO BFS는 최단 거리 알고리즘이 아니므로 Dijkstra/A*로 바꿔야 한다.

경로 막힘 복구는 즉시 재계획 → 짧은 양보 → 대안 목표 → 제한 횟수 후 안전 귀가 순이다. 매 tick 무한 재계획과 막힌 한 item 집착을 금지한다. 이동은 속도 기반 보간, 행동 판단은 tile 도착/사건/deadline 단위로 분리한다.

## 6. 실행한 구조 검증과 한계

이번 작성 시 아래 독립 Python geometry 검사기로 seed1..10000을 실행했다. 24×24에서 2방5000개/3방5000개를 생성했고 10,000개 모두 바닥 전체 연결, 계단 도달, 시작≠계단, 경계 벽 조건을 통과했다. 관측 최대 바닥105타일, 시작→계단 BFS 거리50이다.

이것은 새 생성 구조의 타당성 검증이다. dungeon-mystery 실행·빌드, pmd-bot emulator 실행, 완성 제품·원작 생성기·CPU/RAM 벤치마크 결과가 아니다. 이전 V8 실험과 방 offset 선택 방식이 일부 다르므로 해당 최대101/48 결과를 이번 값과 혼용하지 않는다.

24×24 byte terrain+room 배열은 1152bytes, int32 parent+queue는 4608bytes라는 계산이 가능하다. 이것은 배열 payload 크기이며 allocator,entity,event,graphics,process RSS를 포함한 RAM 측정치가 아니다.

기준 테스트: 10,000 deterministic seeds에서 연결성/다른 방 계단/경계 유지; 원본 출현 가중치에 대한 추첨 통계; inventory once; unreachable items; 친구 blocker; 층 전환·저장 복원 후 RNG 재현. 통로·방 배치 검사는 아래로 재현 가능하나 나머지는 아직 미실행이다.

## 부록 — 재현 가능한 geometry 검사 코드

이 코드는 연구용 실험이며 프로젝트 제품 소스에 추가하지 않았다. 실제 런타임 포팅 시 uint32 overflow,좌표계,큐 범위와 RNG 상태 저장을 별도 검증한다.

~~~python
from collections import deque, Counter
import json

W = H = 24

def generate(seed):
    state = seed
    def rand(n):
        nonlocal state
        state = (1664525 * state + 1013904223) & 0xFFFFFFFF
        return state % n
    slots = [(1, 1), (13, 1), (1, 13), (13, 13)]
    for i in range(3, 0, -1):
        j = rand(i + 1)
        slots[i], slots[j] = slots[j], slots[i]
    count = 2 + rand(2)
    grid = bytearray(W * H)
    centers = []
    for sx, sy in slots[:count]:
        w, h = 3 + rand(4), 3 + rand(4)
        x, y = sx + rand(11 - w), sy + rand(11 - h)
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                grid[yy * W + xx] = 1
        centers.append((x + w // 2, y + h // 2))
    for a, b in zip(centers, centers[1:]):
        x, y = a
        order = [0, 1] if rand(2) else [1, 0]
        for axis in order:
            target = b[axis]
            while (x if axis == 0 else y) != target:
                grid[y * W + x] = 1
                if axis == 0:
                    x += 1 if target > x else -1
                else:
                    y += 1 if target > y else -1
            grid[y * W + x] = 1
    start = centers[0][1] * W + centers[0][0]
    dist = [-1] * (W * H)
    dist[start] = 0
    q = deque([start])
    visited = 0
    while q:
        p = q.popleft()
        visited += 1
        x, y = p % W, p // W
        for dx, dy in [(0, 1), (1, 0), (0, -1), (-1, 0)]:
            nx, ny = x + dx, y + dy
            if 0 <= nx < W and 0 <= ny < H:
                n = ny * W + nx
                if grid[n] and dist[n] == -1:
                    dist[n] = dist[p] + 1
                    q.append(n)
    stairs = max((cy * W + cx for cx, cy in centers[1:]), key=lambda p: dist[p])
    assert visited == sum(grid)
    assert dist[stairs] > 0 and stairs != start
    assert all(grid[x] == grid[(H - 1) * W + x] == 0 for x in range(W))
    assert all(grid[y * W] == grid[y * W + W - 1] == 0 for y in range(H))
    return count, sum(grid), dist[stairs]

if __name__ == '__main__':
    counts = Counter()
    max_floor = max_distance = 0
    for seed in range(1, 10001):
        n, floor, distance = generate(seed)
        counts[n] += 1
        max_floor = max(max_floor, floor)
        max_distance = max(max_distance, distance)
    print(json.dumps({'seeds': [1, 10000], 'passed': 10000,
                      'roomCounts': dict(counts), 'maxFloorTiles': max_floor,
                      'maxStartStairsDistance': max_distance}))
~~~
