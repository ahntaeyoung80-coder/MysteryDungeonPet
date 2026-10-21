"""선택 맵의 원작 shop/MH/trap 문맥을 별도 MDER1로 보존한다."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from tools.mdpet.b1 import load_catalogue
from tools.mdpet.catalogue import build_floor, cdf_weights


def prepare(root, output):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.util import get_ppmdu_config_for_rom, get_binary_from_rom
    from skytemple_files.hardcoded.dungeons import HardcodedDungeons
    source = root/'assets/original/source/user-supplied.nds'
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    rom = NintendoDSRom.fromFile(str(source)); config = get_ppmdu_config_for_rom(rom)
    a = load_catalogue(root/'assets/original/a1-original-package.zip')
    if a['identity']['romHash'] != digest: raise ValueError('original catalogue ROM mismatch')
    definitions = HardcodedDungeons.get_dungeon_list(get_binary_from_rom(rom, config.bin_sections.arm9), config)
    mappa = FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'))
    data = bytearray(struct.pack('<2I', 0x5245444D, 1))
    def u32(n): data.extend(struct.pack('<I', n))
    def string(s):
        b=s.encode('utf-8');u32(len(b));data.extend(b)
    def weights(rows):
        u32(len(rows))
        for row in rows:
            u32(row['originalId']);data.extend(struct.pack('<Q',row['relativeWeight']))
    def categories(context):
        entries=context['categories']['entries'];u32(len(entries))
        conditional={c['categoryId']:c['entries'] for c in context['conditionalItems']}
        for c in entries:
            u32(c['originalId']);data.extend(struct.pack('<Q',c['relativeWeight']));weights(conditional.get(c['originalId'],[]))
    string(digest);u32(2);floors=[];maps=[]
    for identity in (15,18):
        package=root/f'assets/original/exploration/dungeon-{identity}/exploration.mdpe'
        raw=package.read_bytes();length,=struct.unpack_from('<I',raw,8);version=raw[12:12+length].decode('ascii')
        path=f'dungeon-{identity}/exploration.mdpe';u32(identity);string(path);string(version)
        maps.append({'dungeonId':identity,'path':path,'assetVersion':version,'completionVerified':16 if identity==15 else 19})
        d=definitions[identity]
        for local in range(d.number_floors):
            floors.append(build_floor(config.game_edition,identity,local,d,mappa.floor_lists,a['pokemon'],a['items'],{int(k):v for k,v in a['itemCategoryMembers'].items()}))
    u32(len(floors))
    for p in floors:
        key=p['key'];layout=p['rawLayout'];u32(key['dungeonId']);u32(key['localFloor'])
        for name in ('kecleon_shop_chance','monster_house_chance','trap_density'):u32(layout[name])
        weights(p['monsters']['monsterHouse']['entries'])
        trap=sorted((int(k),v) for k,v in p['traps'].items());relative=cdf_weights([v for _,v in trap])
        weights([{'originalId':k,'relativeWeight':weight} for (k,_),weight in zip(trap,relative)])
        for context in ('shop_items','monster_house_items'):
            categories(next(c for c in p['itemContexts'] if c['context']==context))
    output.mkdir(parents=True,exist_ok=True)
    (output/'contexts.mder').write_bytes(data)
    (output/'provenance.json').write_text(json.dumps({'romSha256':digest,'maps':maps,'floors':floors,'policy':'original relative weights retained; product event rate is separate'},ensure_ascii=False,indent=2)+'\n')
    if hashlib.sha256(source.read_bytes()).hexdigest()!=digest:raise ValueError('ROM changed')
    return {'maps':len(maps),'floors':len(floors),'bytes':len(data),'romUnchanged':True}


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path('.'));p.add_argument('--out',type=Path,default=Path('assets/original/gameplay/events'))
    a=p.parse_args();print(json.dumps(prepare(a.root,a.out)))
