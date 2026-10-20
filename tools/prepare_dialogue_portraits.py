"""제공 ROM의 대화 초상화 네 개를 선택 MDPA로 추출한다."""
import argparse
import hashlib
import json
from pathlib import Path

from tools.package_format import encode_package


def prepare(root, output):
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
    # Explicitly checked against the retained body actors. The portrait archive
    # includes alternate forms in a different order: Kecleon is NOT MD383-1.
    roles = [(1, 'Player', 1, 0), (2, 'Partner', 4, 3),
             (3, 'FriendlyKecleon', 383, 373), (4, 'Loudred', 322, 321)]
    atlas = Image.new('RGBA', (160, 40))
    clips, audit = [], []
    for column, (role, name, md, index) in enumerate(roles):
        portrait = archive.get(index, 0)
        if portrait is None:
            raise ValueError(f'missing original portrait: {name}, KAO{index}:0')
        image = portrait.get().convert('RGBA')
        if image.size != (40, 40):
            raise ValueError('unexpected original portrait dimensions')
        atlas.paste(image, (column * 40, 0))
        metadata = {'role': role, 'name': name, 'bodyMdIndex': md,
                    'kaoIndex': index, 'expression': 0, 'rgbaSha256': digest(image.tobytes())}
        audit.append(metadata)
        clips.append({'id': role, 'name': name, 'rows': [[{
            'rect': [column * 40, 0, 40, 40], 'anchor': [0, 0], 'shadow': [0, 0],
            'ticks': 1, 'markers': [], 'sourceMetadata': json.dumps(metadata, sort_keys=True)}]]})
    if digest(source.read_bytes()) != before:
        raise ValueError('ROM changed')
    output.mkdir(parents=True)
    atlas.save(output / 'atlas.png')
    png = (output / 'atlas.png').read_bytes()
    provenance = {'romSha256': before, 'sourcePath': 'FONT/kaomado.kao',
                  'sourceSha256': digest(raw), 'portraits': audit,
                  'mapping': 'explicit original body/portrait visual comparison; archive indices differ from MD and sprite indices',
                  'transform': 'none; original 40x40 indexed pixels converted to RGBA, no resize or mirroring'}
    package = {'assetVersion': 1, 'tickHz': 60, 'width': 160, 'height': 40,
               'pngBytes': len(png), 'sourceKind': 1, 'atlasName': 'atlas.png',
               'pngSha256': digest(png), 'assetKey': 'rom:font:dialogue-portraits',
               'provenance': json.dumps(provenance, sort_keys=True), 'clips': clips}
    (output / 'portraits.mdpa').write_bytes(encode_package(package))
    (output / 'provenance.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (output / 'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original ROM dialogue portraits\n', encoding='utf-8')
    return {'romUnchanged': True, 'originalPortraits': audit, 'output': str(output)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.root, args.out), ensure_ascii=False, indent=2))
