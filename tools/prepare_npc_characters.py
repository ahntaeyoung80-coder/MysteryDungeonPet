"""원작 MD별 NPC 몸체·명시 초상화와 방문 동작을 선택 패키지로 만든다."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from tools.prepare_party_actor import prepare as prepare_body
from tools.prepare_dialogue_portraits import prepare as prepare_portraits, body_identity
from tools.mdpet.common import staging


def prepare(root, output, selection_path=None):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    source = root/'assets/original/source/user-supplied.nds'
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    before = digest(source)
    rom = NintendoDSRom.fromFile(str(source))
    md = FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md'))
    kao = FileType.KAO.deserialize(rom.getFileByName('FONT/kaomado.kao'))
    talk = FileType.TBL_TALK.deserialize(rom.getFileByName('MESSAGE/tbl_talk.tlk'))
    gameplay = root/'assets/original/gameplay'
    selection = json.loads((selection_path or gameplay/'npcs/selection.json').read_text())
    if selection['schemaVersion'] != 1:
        raise ValueError('unsupported NPC selection')
    starter = json.loads((gameplay/'starters/catalogue.json').read_text())
    shared = {}
    for row in starter['characters']:
        src, _, _, _ = body_identity(gameplay/row['bodyManifest'])
        if src['romHash'] != before:
            raise ValueError('shared starter ROM mismatch')
        shared[src['spriteIndex']] = row['bodyManifest']
    for name in ('asset-2', 'asset-3'):
        path = f'actors/{name}/asset.mdpa'
        src, _, _, _ = body_identity(gameplay/path)
        if src['romHash'] != before:
            raise ValueError('shared compatibility actor ROM mismatch')
        shared[src['spriteIndex']] = path
    # Reuse already verified original NPC bodies when widening the selected maps.
    existing_proofs = {}
    existing = gameplay/'npcs/catalogue.json'
    if existing.exists():
        for row in json.loads(existing.read_text())['characters']:
            existing_proofs[row['mdIndex']] = row.get('verification')
            src, _, _, _ = body_identity(gameplay/row['bodyManifest'])
            if src['romHash'] != before:
                raise ValueError('existing NPC body ROM mismatch')
            shared[src['spriteIndex']] = row['bodyManifest']
    supplement_masks = {}
    supplement_root = root/'assets/original/source/portrait-supplements'
    for entry in json.loads((supplement_root/'manifest.json').read_text())['files']:
        file = (supplement_root/entry['file']).resolve()
        if not file.is_relative_to(supplement_root.resolve()) or digest(file) != entry['sha256']:
            raise ValueError('NPC portrait supplement provenance mismatch')
        supplement_masks[entry['mdIndex']] = supplement_masks.get(entry['mdIndex'],0) | (1<<entry['expression'])
    entries, mappings, seen = [], [], set()
    with staging(output) as temporary:
        for row in selection['characters']:
            index = row['mdIndex']
            if index in seen or not 0 < index < len(md.entries):
                raise ValueError('duplicate/out-of-range NPC MD')
            seen.add(index)
            record = md.entries[index]
            sprite = int(record.sprite_index)
            if sprite < 0 or record.national_pokedex_number <= 0:
                raise ValueError('NPC has no original body')
            kao_index = row['kaoIndex']
            if not 0 <= kao_index < kao.n_entries() or kao.get(kao_index, 0) is None:
                raise ValueError('selected NPC has no verified Normal portrait')
            mask = sum(1 << i for i in range(14) if kao.get(kao_index, i*2))
            verification = existing_proofs.get(index)
            if sprite not in shared:
                dest = temporary/'npcs/bodies'/str(sprite)
                verification = prepare_body(root, dest, index)
                shared[sprite] = f'npcs/bodies/{sprite}/asset.mdpa'
            manifest = shared[sprite]
            path = temporary/manifest if (temporary/manifest).is_file() else gameplay/manifest
            src, key, body_hash, _ = body_identity(path)
            if src['spriteIndex'] != sprite or src['nationalDex'] != record.national_pokedex_number or src['romHash'] != before:
                raise ValueError('shared body source does not match logical MD sprite/species')
            entry = dict(row, speciesId=int(record.national_pokedex_number), spriteIndex=sprite,
                         originalEmotionMask=mask, emotionMask=mask | supplement_masks.get(index,0), talkGroup=int(talk.get_monster_personality(index)),
                         characterId=f'rom:{before}:{src["edition"]}:md:{index}',
                         bodyAssetKey=key, bodyAtlasSha256=body_hash, bodyManifest=manifest,
                         verification=verification)
            entries.append(entry)
            mappings.append({k: row[k] for k in ('mdIndex','kaoIndex','displayName')})
        characters = json.loads((gameplay/'portraits/characters.json').read_text())
        known = {r['mdIndex']: r for r in characters['characters']}
        for row in mappings:
            if row['mdIndex'] in known and row != known[row['mdIndex']]:
                raise ValueError('conflicting explicit portrait mapping')
            known[row['mdIndex']] = row
        characters['characters'] = list(sorted(known.values(), key=lambda r:r['mdIndex']))
        mapping_path = temporary/'characters.json'
        mapping_path.write_text(json.dumps(characters,ensure_ascii=False,indent=2)+'\n')
        prepare_portraits(root, temporary/'portraits', characters_path=mapping_path, expressions=True)
        mapping_path.unlink()
        data = bytearray(struct.pack('<2I',0x434e444d,1))  # MDNC1; separate from MDES/MDPE.
        def u32(value): data.extend(struct.pack('<I',value))
        def string(value):
            encoded=value.encode('utf-8')
            if len(encoded)>256: raise ValueError('NPC catalogue string limit')
            u32(len(encoded));data.extend(encoded)
        string(before);u32(len(entries))
        for entry in entries:
            for field in ('mdIndex','speciesId','spriteIndex','kaoIndex','emotionMask','talkGroup','visitorActorId'):
                u32(entry.get(field,0))
            for field in ('characterId','bodyAssetKey','bodyAtlasSha256','bodyManifest','displayName'):
                string(entry[field])
        (temporary/'npcs/characters.mdnc').write_bytes(data)
        (temporary/'npcs/selection.json').write_text(json.dumps(selection,ensure_ascii=False,indent=2)+'\n')
        audit={'schemaVersion':1,'romSha256':before,'characters':entries,
               'scope':selection.get('scope','selected dungeon normal/MH/fixed union, legacy actors and guild visitors'),
               'portraitMapping':'explicit KAO rows; original Normal fallback; no role/relationship-based body substitute'}
        (temporary/'npcs/catalogue.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
        (temporary/'npcs/CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original ROM NPC bodies and portraits\n')
        if before != digest(source):
            raise ValueError('ROM changed')
    return {'romUnchanged':True,'characters':len(entries),'newBodyPackages':len(list((output/'npcs/bodies').glob('*'))),
            'comparedFrames':sum((e['verification'] or {}).get('comparedFrames',0) for e in entries)}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(prepare(args.root,args.out),ensure_ascii=False))
