"""원작 조사 ID/층/경로를 검증하고 선택 원정 패키지를 생성한다."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import struct

AUDIT = 'docs/research/2026-10-21-full-map-audit'
ROUTES = 'docs/research/2026-10-21-continuous-expeditions'
# 조사 목록 전체와 실행에 채택한 경로를 구별한다. 퍼즐/미확정 연결은 제외한다.
SELECTED = ('RUN-D014', 'RUN-D017', 'RUN-D001', 'RUN-D004',
            'RUN-D021', 'RUN-D035', 'RUN-D041', 'ARC-M07')


def catalogue(root):
    audit = root / AUDIT
    source = root / ROUTES
    read = lambda p: json.loads(p.read_text(encoding='utf-8'))
    routes = read(source/'routes.json')
    maps = {m['dungeonId']: m for m in read(audit/'map-gameplay.json')}
    fixed = {m['fixedFloorId']: m for m in read(audit/'gameplay-fixed-spawns.json')}
    physical = {p['physicalId']: p for p in map(json.loads, gzip.open(audit/'gameplay-floor-profiles.jsonl.gz', 'rt'))}
    runs = {r['id']: r for r in routes['sectionRoutes'] + routes['storyArcs']}
    links = {l['id']: l for l in routes['links']}
    grounds = {g['id']: g for g in read(source/'grounds.json')}
    plans = []
    for identity in SELECTED:
        run = runs[identity]
        rows = []
        for index, dungeon in enumerate(run['dungeonIds']):
            m = maps[dungeon]
            if m['usage'] != 'dungeon' or m['missingReferences'] or m['resolvedFloors'] != m['declaredFloors']:
                raise ValueError(f'unresolved/non-playable segment {dungeon}')
            link = None
            if index+1 < len(run['dungeonIds']):
                link = links[run['links'][index]]
                if (link['fromDungeonId'], link['toDungeonId']) != (dungeon, run['dungeonIds'][index+1]):
                    raise ValueError('route/link order mismatch')
                if link['condition'] != 'forward_success' or link['guildReturnBeforeNext'] or '~' in link['id']:
                    raise ValueError('non-forward link cannot be selected automatically')
                if link['groundResolution'] not in ('confirmed_path', 'no_separate_connector_assigned'):
                    raise ValueError('unresolved connector')
            for floor in m['floorRefs']:
                p = physical[floor['physicalId']]
                if floor['localFloor0']+m['startAfter'] != p['groupFloor']:
                    raise ValueError('local/group floor mismatch')
                fid = floor['fixedFloorId']
                row = {'dungeonId': dungeon, 'localFloor': floor['localFloor0'], 'name': m['name'],
                       'physicalId': floor['physicalId'], 'fixedFloorId': fid,
                       'fixedSpawns': fixed[fid]['spawns'] if fid else [],
                       'revisitFixedFloorId': fixed[fid]['overrideFixedFloorId'] if fid else 0,
                       'revisitSpawns': fixed.get(fixed[fid]['overrideFixedFloorId'], {}).get('spawns', []) if fid else [],
                       'profile': dict(p, key={'edition': p['edition'], 'dungeonId': dungeon, 'localFloor': floor['localFloor0']}),
                       'exitLink': '', 'groundIds': []}
                if link and floor['localFloor0']+1 == m['declaredFloors']:
                    row.update(exitLink=link['id'], groundIds=link['viaGroundIds'])
                    for ground in row['groundIds']:
                        if ground not in grounds:
                            raise ValueError('connector ground is absent')
                rows.append(row)
        if len(rows) > 128:
            raise ValueError('selected expedition exceeds snapshot floor budget')
        entry = [c['groundId'] for c in routes['connectors']
                 if c['kind'] == 'entry' and c['toDungeonId'] == run['dungeonIds'][0]]
        plans.append({'id': identity, 'name': run['name'], 'dungeonIds': run['dungeonIds'],
                      'entryGroundIds': entry, 'floors': rows})
    return {'schemaVersion': 1, 'romHash': read(audit/'gameplay-extraction.json')['romSha256'],
            'plans': plans, 'grounds': grounds,
            'branches': [b for b in routes['branches'] if b['viaDungeonId'] in (9, 11, 13)],
            'excluded': {'nonDungeon': [m['dungeonId'] for m in maps.values() if m['usage'] != 'dungeon'],
                         'unresolved': routes['unresolved'],
                         'unselectedRoutes': sorted(set(runs)-set(SELECTED))}}


class Writer:
    def __init__(self): self.data = bytearray()
    def u32(self, value): self.data.extend(struct.pack('<I', value))
    def string(self, text):
        raw = text.encode('utf-8')
        if len(raw) > 256: raise ValueError('expedition string exceeds limit')
        self.u32(len(raw)); self.data.extend(raw)


def write_plan(plan, stops):
    w = Writer()
    def write_stops(ids):
        w.u32(len(ids))
        for identity in ids:
            s = stops[identity]
            w.string(identity); w.string(s['assetFile'])
            for n in (*s['size'], *s['player'], *s['partner'], int(s.get('storage', False))): w.u32(n)
    w.string(plan['id']); w.string(plan['name']); w.u32(len(plan['floors']))
    for f in plan['floors']:
        for n in (f['dungeonId'], f['localFloor'], f['fixedFloorId'], f.get('revisitFixedFloorId', 0)): w.u32(n)
        for field in ('fixedSpawns', 'revisitSpawns'):
            spawns = [s for s in f.get(field, []) if s['mdIndex'] or s['itemId']]
            w.u32(len(spawns))
            for s in spawns:
                for n in (s['mdIndex'], s['itemId'], s['quantity'], s['x'], s['y'], s['spawnType']): w.u32(n)
        tiles=f.get('fixedTiles',[]);w.u32(len(tiles))
        for tile in tiles:w.u32(tile)
        for value in (f.get('fixedStart',0),f.get('fixedExit',0),f.get('backgroundX',0)+4096,f.get('backgroundY',0)+4096):w.u32(value)
        w.string(f['exitLink']); write_stops(f['groundIds'])
    write_stops(plan['entryGroundIds'])
    for n in (0, 0, bool(plan['entryGroundIds']), 0, 0): w.u32(n)
    return bytes(w.data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--audit-only', action='store_true')
    parser.add_argument('--report', type=Path, default=Path('build/expeditions/catalogue.json'))
    args = parser.parse_args()
    result = catalogue(args.root)
    summary = {k: v for k, v in result.items() if k not in ('grounds', 'plans')}
    summary['plans'] = [{k: v for k, v in p.items() if k != 'floors'} | {'floors': len(p['floors'])} for p in result['plans']]
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(summary, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps({'plans': len(result['plans']), 'uniqueSegments': len({d for p in result['plans'] for d in p['dungeonIds']}),
                      'status': 'classification validated; runtime integration pending'}, ensure_ascii=False))


if __name__ == '__main__': main()
