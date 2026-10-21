"""선택 던전의 전체 floor_items를 원작 item record의 sprite/palette에 연결한다."""
import argparse
import hashlib
import json
from pathlib import Path
from tools.mdpet.b1 import load_catalogue
from tools.package_format import encode_package


def selected_ids(root):
    floors = json.loads((root/'assets/original/gameplay/events/provenance.json').read_text())['floors']
    floors += [p for p in load_catalogue(root/'assets/original/a1-original-package.zip')['floors'] if p['key']['dungeonId'] == 0]
    result = set()
    for p in floors:
        context = next(c for c in p['itemContexts'] if c['context'] == 'floor_items')
        categories = {c['originalId'] for c in context['categories']['entries'] if c['relativeWeight']}
        for category in context['conditionalItems']:
            if category['categoryId'] in categories:
                result.update(row['originalId'] for row in category['entries'] if row['relativeWeight'])
        result.update(g['recordIndex'] for g in context['guaranteed'])
    # Historic exported GUI saves used these real item records as generic items.
    # Keep their own graphics without adding them to any spawn distribution.
    return sorted(result | {10, 11})


def prepare(root, output):
    from PIL import Image
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.util import get_ppmdu_config_for_rom
    source = root/'assets/original/source/user-supplied.nds'
    digest = lambda b: hashlib.sha256(b).hexdigest()
    before = digest(source.read_bytes())
    rom = NintendoDSRom.fromFile(str(source))
    raw = rom.getFileByName('DUNGEON/dungeon.bin')
    pack = FileType.DUNGEON_BIN.deserialize(raw, get_ppmdu_config_for_rom(rom))
    raw_items = rom.getFileByName('BALANCE/item_p.bin')
    records = FileType.ITEM_P.deserialize(raw_items).item_list
    cells, images, bindings, clips = {}, [], [], []
    for identity in selected_ids(root):
        item = records[identity]
        key = (int(item.sprite), int(item.palette))
        if key not in cells:
            indexed = pack[1022].to_pil(*key)
            image = indexed.convert('RGBA')
            image.putalpha(Image.frombytes('L', image.size, bytes(0 if x % 16 == 0 else 255 for x in indexed.tobytes())))
            if image.size != (16, 16):
                raise ValueError('original item sprite size mismatch')
            cells[key] = len(images)
            images.append(image)
        cell = cells[key]
        clips.append({'id': identity, 'name': f'Item{identity}', 'rows': [[{'rect': [cell % 16 * 16, cell // 16 * 16, 16, 16], 'anchor': [8, 8], 'shadow': [0, 0], 'ticks': 1, 'markers': []}]]})
        bindings.append({'itemId': identity, 'sprite': key[0], 'palette': key[1], 'cell': cell, 'rgbaSha256': digest(images[cell].tobytes())})
    output.mkdir(parents=True, exist_ok=True)
    atlas = Image.new('RGBA', (256, ((len(images)+15)//16)*16))
    for cell, image in enumerate(images):
        atlas.paste(image, (cell % 16 * 16, cell // 16 * 16))
    atlas.save(output/'atlas.png')
    package = {'assetVersion': 1, 'tickHz': 60, 'width': atlas.width, 'height': atlas.height, 'pngBytes': (output/'atlas.png').stat().st_size,
               'sourceKind': 1, 'atlasName': 'atlas.png', 'pngSha256': digest((output/'atlas.png').read_bytes()), 'assetKey': 'rom:EoS_NA:floor-items',
               'provenance': json.dumps({'romHash': before, 'source': 'DUNGEON/dungeon.bin:1022 + BALANCE/item_p.bin', 'scope': 'dungeon0/15/18 complete floor_items union + historic records10/11; no extra item effects'}), 'clips': clips}
    (output/'items.mdpa').write_bytes(encode_package(package))
    (output/'provenance.json').write_text(json.dumps({'romSha256': before, 'itemSourceSha256': digest(raw_items), 'dungeonSourceSha256': digest(raw), 'compatibilityItemIds': [10, 11], 'items': bindings, 'uniqueSprites': len(images)}, ensure_ascii=False, indent=2)+'\n')
    (output/'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original item graphics\n')
    if before != digest(source.read_bytes()):
        raise ValueError('ROM changed')
    return {'items': len(bindings), 'uniqueSprites': len(images), 'romUnchanged': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.root, args.out), ensure_ascii=False))
