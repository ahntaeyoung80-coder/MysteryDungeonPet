"""제공 ROM의 대화 초상화 네 개를 선택 MDPA로 추출한다."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from tools.package_format import encode_package


def body_identity(path):
    data = path.read_bytes()
    if struct.unpack_from('<2I', data) != (0x4150444D, 1):
        raise ValueError('invalid body package header')
    offset, fields = 36, []
    for _ in range(4):
        length = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        fields.append(data[offset:offset + length].decode('utf-8'))
        offset += length
    source = json.loads(fields[3])['source']
    character = f"rom:{source['romHash']}:{source['edition']}:md:{source['mdIndex']}"
    return source, fields[2], fields[1], character


def prepare(root, output, cast_path=None, characters_path=None, expressions=False):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from PIL import Image
    if output.exists():
        raise ValueError('choose a new output directory')
    source = root / 'assets/original/source/user-supplied.nds'
    digest = lambda data: hashlib.sha256(data).hexdigest()
    before = digest(source.read_bytes())
    rom = NintendoDSRom.fromFile(str(source))
    raw = rom.getFileByName('FONT/kaomado.kao')
    archive = FileType.KAO.deserialize(raw)
    cast_path = cast_path or root / 'assets/original/gameplay/portraits/cast.json'
    config = json.loads(cast_path.read_text(encoding='utf-8'))
    if config['schemaVersion'] != 1 or sorted(a['role'] for a in config['actors']) != [1, 2, 3, 4]:
        raise ValueError('cast requires exactly one player, partner, friendly and alarm role')
    characters_path = characters_path or root / 'assets/original/gameplay/portraits/characters.json'
    catalogue = json.loads(characters_path.read_text(encoding='utf-8'))
    if catalogue['schemaVersion'] != 1 or catalogue['romSha256'] != before:
        raise ValueError('portrait character catalogue ROM/schema mismatch')
    known = {c['mdIndex']: c for c in catalogue['characters']}
    if len(known) != len(catalogue['characters']):
        raise ValueError('duplicate portrait character ID')
    actors, characters = [], {}
    for actor in config['actors']:
        manifest = Path(actor['bodyManifest'])
        if manifest.is_absolute() or any(p in ('.', '..') for p in manifest.parts) or '\\' in str(manifest) or ':' in str(manifest):
            raise ValueError('unsafe cast body path')
        src, body_key, body_hash, character = body_identity(root / 'assets/original/gameplay' / manifest)
        if src['romHash'] != before:
            raise ValueError('body and portrait ROM provenance differ')
        md = src['mdIndex']
        if md not in known or src['edition'] != catalogue['edition']:
            raise ValueError(f'no verified portrait mapping for character MD{md}; add a character catalogue entry')
        definition = known[md]
        if not 0 < md <= 65535 or not definition['displayName']:
            raise ValueError('invalid cast character')
        entry = {'characterId': character, 'bodyMdIndex': md, 'kaoIndex': definition['kaoIndex']}
        if md in characters and characters[md] != entry:
            raise ValueError('one character has conflicting portrait mappings')
        characters[md] = entry
        actors.append({'role': actor['role'], 'bodyManifest': actor['bodyManifest'],
                       'displayName': definition['displayName'], 'characterId': character,
                       'bodyAssetKey': body_key, 'bodyAtlasSha256': body_hash, 'portraitClip': md})
    config = {'schemaVersion': 1, 'actors': [{'role': a['role'], 'bodyManifest': a['bodyManifest']} for a in actors]}
    if expressions:
        characters = {md: {'characterId': f"rom:{before}:{catalogue['edition']}:md:{md}", 'bodyMdIndex': md, 'kaoIndex': row['kaoIndex']} for md,row in known.items()}
    atlas = Image.new('RGBA', (1280, 40*((len(characters)*(14 if expressions else 1)+31)//32)))
    portrait_cells = {}
    supplements = {}
    supplement_root = root / 'assets/original/source/portrait-supplements'
    supplement_manifest = supplement_root / 'manifest.json'
    if expressions and supplement_manifest.exists():
        supplement_data = json.loads(supplement_manifest.read_text())
        for entry in supplement_data['files']:
            path = supplement_root / entry['file']
            if not path.resolve().is_relative_to(supplement_root.resolve()):
                raise ValueError('unsafe portrait supplement path')
            if digest(path.read_bytes()) != entry['sha256']:
                raise ValueError('portrait supplement hash mismatch')
            supplements[(entry['mdIndex'], entry['expression'])] = entry
    clips, audit = [], []
    for column, (md, character) in enumerate(sorted(characters.items())):
        index = character['kaoIndex']
        frames = []
        for expression in range(14 if expressions else 1):
            actual = expression if archive.get(index, expression*2) else 0
            portrait = archive.get(index, actual*2)
            if portrait is None:
                raise ValueError(f'missing original portrait: MD{md}, KAO{index}:0')
            supplement = supplements.get((md, expression)) if actual != expression else None
            image = Image.open(supplement_root / supplement['file']).convert('RGBA') if supplement else portrait.get().convert('RGBA')
            if supplement: actual = expression
            if image.size != (40, 40):
                raise ValueError('unexpected original portrait dimensions')
            pixels = image.tobytes()
            if pixels not in portrait_cells:
                cell = len(portrait_cells); portrait_cells[pixels] = cell
                atlas.paste(image, (cell%32*40, cell//32*40))
            cell = portrait_cells[pixels]
            metadata = {'characterId': character['characterId'], 'bodyMdIndex': md,
                        'kaoIndex': index, 'expression': expression, 'actualExpression': actual,
                        'rgbaSha256': digest(image.tobytes()), 'portraitSource': supplement if supplement else 'original ROM'}
            audit.append(metadata)
            frames.append({'rect': [cell%32*40, cell//32*40, 40, 40], 'anchor': [0,0], 'shadow': [0,0],
                           'ticks': 1, 'markers': [], 'sourceMetadata': json.dumps(metadata, sort_keys=True)})
        clips.append({'id': md, 'name': f'MD{md}', 'sourceMetadata': character['characterId'], 'rows': [frames]})
    if digest(source.read_bytes()) != before:
        raise ValueError('ROM changed')
    output.mkdir(parents=True)
    atlas = atlas.crop((0,0,1280,40*((len(portrait_cells)+31)//32)))
    atlas.save(output / 'atlas.png')
    png = (output / 'atlas.png').read_bytes()
    provenance = {'romSha256': before, 'sourcePath': 'FONT/kaomado.kao',
                  'sourceSha256': digest(raw), 'portraits': audit, 'cast': actors,
                  'mapping': 'explicit original body/portrait visual comparison; archive indices differ from MD and sprite indices',
                  'transform': 'none; original 40x40 indexed pixels converted to RGBA, no resize or mirroring'}
    provenance_text = json.dumps(provenance, ensure_ascii=False, indent=2) + '\n'
    runtime_provenance = {'romSha256': before, 'sourcePath': 'FONT/kaomado.kao', 'sourceSha256': digest(raw), 'auditFile': 'provenance.json', 'auditSha256': digest(provenance_text.encode('utf-8'))} if expressions else provenance
    package = {'assetVersion': 1, 'tickHz': 60, 'width': atlas.width, 'height': atlas.height,
               'pngBytes': len(png), 'sourceKind': 2 if supplements else 1, 'atlasName': 'atlas.png',
               'pngSha256': digest(png), 'assetKey': 'rom:font:dialogue-portraits',
               'provenance': json.dumps(runtime_provenance, sort_keys=True), 'clips': clips}
    (output / 'portraits.mdpa').write_bytes(encode_package(package))
    binding = bytearray(struct.pack('<3I', 0x4344444D, 1, len(actors)))
    for actor in sorted(actors, key=lambda a: a['role']):
        binding.extend(struct.pack('<2I', actor['role'], actor['portraitClip']))
        for key in ('characterId', 'bodyAssetKey', 'bodyAtlasSha256', 'bodyManifest', 'displayName'):
            value = actor[key].encode('utf-8')
            if len(value) > 256:
                raise ValueError('cast string exceeds runtime limit')
            binding.extend(struct.pack('<I', len(value)) + value)
    (output / 'cast.mddc').write_bytes(binding)
    (output / 'cast.json').write_text(json.dumps(config, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (output / 'characters.json').write_text(json.dumps(catalogue, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (output / 'provenance.json').write_text(provenance_text, encoding='utf-8')
    (output / 'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original ROM dialogue portraits\n', encoding='utf-8')
    if supplements:
        import shutil
        for name in ('LICENSE.CC_BY-NC_4.md','LICENSE.PMDCollab_1.md'):
            shutil.copyfile(supplement_root / name, output / name)
        for md in (322,383):shutil.copyfile(supplement_root / str(md) / 'credits.txt', output / f'credits-MD{md}.txt')
        with (output / 'CREDITS.txt').open('a',encoding='utf-8') as credits:
            credits.write('Selected missing expressions: PMDCollab/SpriteCollab e4b1949687cb2344e6675717466e357189d3acba; see per-character credits and licenses.\nhttps://github.com/PMDCollab/SpriteCollab/tree/e4b1949687cb2344e6675717466e357189d3acba/portrait\n')
    return {'romUnchanged': True, 'originalPortraits': audit, 'cast': actors, 'output': str(output)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cast', type=Path)
    parser.add_argument('--characters', type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.root, args.out, args.cast, args.characters), ensure_ascii=False, indent=2))
