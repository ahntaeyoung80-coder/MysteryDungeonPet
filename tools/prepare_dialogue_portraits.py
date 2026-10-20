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


def prepare(root, output, cast_path=None, characters_path=None):
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
    atlas = Image.new('RGBA', (40 * len(characters), 40))
    clips, audit = [], []
    for column, (md, character) in enumerate(sorted(characters.items())):
        index = character['kaoIndex']
        portrait = archive.get(index, 0)
        if portrait is None:
            raise ValueError(f'missing original portrait: MD{md}, KAO{index}:0')
        image = portrait.get().convert('RGBA')
        if image.size != (40, 40):
            raise ValueError('unexpected original portrait dimensions')
        atlas.paste(image, (column * 40, 0))
        metadata = {'characterId': character['characterId'], 'bodyMdIndex': md,
                    'kaoIndex': index, 'expression': 0, 'rgbaSha256': digest(image.tobytes())}
        audit.append(metadata)
        clips.append({'id': md, 'name': f'MD{md}', 'sourceMetadata': character['characterId'], 'rows': [[{
            'rect': [column * 40, 0, 40, 40], 'anchor': [0, 0], 'shadow': [0, 0],
            'ticks': 1, 'markers': [], 'sourceMetadata': json.dumps(metadata, sort_keys=True)}]]})
    if digest(source.read_bytes()) != before:
        raise ValueError('ROM changed')
    output.mkdir(parents=True)
    atlas.save(output / 'atlas.png')
    png = (output / 'atlas.png').read_bytes()
    provenance = {'romSha256': before, 'sourcePath': 'FONT/kaomado.kao',
                  'sourceSha256': digest(raw), 'portraits': audit, 'cast': actors,
                  'mapping': 'explicit original body/portrait visual comparison; archive indices differ from MD and sprite indices',
                  'transform': 'none; original 40x40 indexed pixels converted to RGBA, no resize or mirroring'}
    package = {'assetVersion': 1, 'tickHz': 60, 'width': atlas.width, 'height': 40,
               'pngBytes': len(png), 'sourceKind': 1, 'atlasName': 'atlas.png',
               'pngSha256': digest(png), 'assetKey': 'rom:font:dialogue-portraits',
               'provenance': json.dumps(provenance, sort_keys=True), 'clips': clips}
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
    (output / 'provenance.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (output / 'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original ROM dialogue portraits\n', encoding='utf-8')
    return {'romUnchanged': True, 'originalPortraits': audit, 'cast': actors, 'output': str(output)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cast', type=Path)
    parser.add_argument('--characters', type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.root, args.out, args.cast, args.characters), ensure_ascii=False, indent=2))
