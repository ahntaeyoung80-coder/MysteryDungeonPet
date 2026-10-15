"""Reuse the green Kecleon and derive its hostile original-ROM counterpart."""
import argparse
import json
from pathlib import Path
import shutil
import tempfile

from tools.mdpet.common import file_hash, write_json
from tools.prepare_party_actor import prepare


def hashes(folder):
    return {p.relative_to(folder).as_posix(): file_hash(p)
            for p in sorted(folder.rglob('*')) if p.is_file()}


def prepare_relationships(root):
    actors = root / 'assets/original/gameplay/actors'
    rom = root / 'assets/original/source/user-supplied.nds'
    rom_hash = file_hash(rom)
    existing = {name: hashes(actors / name) for name in ('asset-0', 'asset-1', 'asset-2')}
    green = json.loads((actors / 'asset-2/provenance.json').read_text(encoding='utf-8'))
    source = green['source']
    if (source['mdIndex'], source['spriteIndex'], source['nationalDex']) != (383, 556, 352):
        raise ValueError('existing friendly actor is not the retained green Kecleon')
    if source['romHash'] != rom_hash:
        raise ValueError('friendly actor and retained ROM provenance differ')
    with tempfile.TemporaryDirectory() as workspace:
        derived = Path(workspace) / 'asset-3'
        verification = prepare(root, derived, 384)
        if (verification['mdIndex'], verification['spriteIndex'], verification['nationalDex']) != (384, 554, 352):
            raise ValueError(f'unexpected hostile Kecleon record: {verification}')
        expected = {'asset.mdpa', 'atlas.png', 'CREDITS.txt', 'provenance.json'}
        if {p.name for p in derived.iterdir()} != expected:
            raise ValueError('unexpected relationship actor output files')
        shutil.copytree(derived, actors / 'asset-3', dirs_exist_ok=True)
    index_path = actors / 'runtime-index.json'
    index = json.loads(index_path.read_text(encoding='utf-8'))
    roles = {'friendly': 'asset-2/asset.mdpa', 'hostile': 'asset-3/asset.mdpa'}
    index['assets'] = [entry for entry in index['assets']
                       if entry['manifest'] not in roles.values()]
    for role, manifest in roles.items():
        folder = actors / Path(manifest).parent
        provenance = json.loads((folder / 'provenance.json').read_text(encoding='utf-8'))
        src = provenance['source']
        index['assets'].append({
            'assetKey': f"rom:{src['edition']}:monster:{src['mdIndex']}",
            'manifest': manifest, 'sha256': file_hash(folder / 'asset.mdpa'),
            'atlasSha256': file_hash(folder / 'atlas.png'),
            'mdIndex': src['mdIndex'], 'spriteIndex': src['spriteIndex'], 'relationship': role,
        })
    index['relationshipRoles'] = roles
    # The retained base sourceManifestHash still identifies the original 0/1
    # compilation. Each relationship entry has its own ROM provenance and hashes.
    index['relationshipSource'] = 'per-actor provenance.json; retained ROM WAN pixels'
    write_json(index_path, index)
    if file_hash(rom) != rom_hash or any(hashes(actors / name) != before for name, before in existing.items()):
        raise ValueError('retained ROM or an existing actor changed')
    return {
        'status': 'pass', 'romSha256': rom_hash, 'romHashUnchanged': True,
        'existingActorsUnchanged': list(existing), 'originalWanPixelMatch': 'pass',
        'friendly': {'mdIndex': 383, 'spriteIndex': 556, 'manifest': roles['friendly']},
        'hostile': {'mdIndex': 384, 'spriteIndex': 554, 'manifest': roles['hostile']},
        'hostileVerification': verification,
        'originalWallTime': 'pending',
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    result = prepare_relationships(args.root)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    write_json(args.report, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))
