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
