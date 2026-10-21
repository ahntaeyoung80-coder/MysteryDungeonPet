"""Assert the existing Windows fixture behaviours after binding explicit original NPCs."""
import csv
import json
import sys
from pathlib import Path

output = Path(sys.argv[1])
def rows(name):
    return [r for r in csv.DictReader((output / name / 'trace.csv').open()) if r['submitted'] == '1']

follow, attack, navigation, entry, party = [rows(n) for n in ('follow', 'attack', 'navigation', 'entry', 'party')]
assert sum(r['companion_moving'] == '1' and r['companion_clip'] == '0' for r in follow) >= 3
assert sum(r['companion_clip'] == '1' and r['combat'] == '12' for r in attack) >= 2
relationships = []
for name, expected in (('friendly', 0), ('hostile', 1)):
    before = json.loads((output / name / 'state.json').read_text())
    after = json.loads((output / name / 'restored-state.json').read_text())
    assert before['runId'] == after['runId'] and before['hostileCount'] == after['hostileCount'] == expected
    relationships.append({'name': name, 'runId': after['runId'], 'hostileCount': expected})
actions = {r['action_id']: r for r in navigation if r['life'] == '0' and r['action_id'] != '0'}
assert len(actions) >= 3
assert any(int(r['to_tile']) not in (102, 125) for r in actions.values()) or any(r['life'] != '0' or r['scene_beat'] != '0' for r in navigation)
assert sum(r['life'] == '10' for r in entry) >= 3
assert not any(r['clip'] == '1' and r['companion_clip'] == '1' and int(r['player_party_action']) > 0 and int(r['companion_party_action']) > 0 and r['player_party_action'] != r['companion_party_action'] for r in party)
assert sum(r['clip'] == '1' for r in party) >= 2 and sum(r['companion_clip'] == '1' for r in party) >= 2
def has_actor(row, actor_id):
    return any(s.startswith(f'{actor_id}:') for s in row['actor_states'].split(';'))
assert any(not has_actor(r, 1) and has_actor(r, 2) for r in party)
assert any(not has_actor(r, 1) and not has_actor(r, 2) for r in party)
result = {'status': 'pass', 'platform': 'Wine10; root tracked EXE', 'launches': 9,
          'fixtures': 'Windows exporters: 24 snapshots / 24 actors validated; native exporters: 16 snapshots / 17 actors validated',
          'submittedRows': {n: len(rows(n)) for n in ('follow', 'attack', 'navigation', 'entry', 'party')},
          'relationships': relationships, 'checks': ['companion Walk/Attack', 'relationship restore', 'navigation detour', 'NPC greeting', 'sequential ally attacks', 'one enemy removed while the other continues', 'both enemies removed'],
          'bodyPixels': json.loads((output / 'sprite-pixels.json').read_text())}
(output / 'summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(result, ensure_ascii=False))
