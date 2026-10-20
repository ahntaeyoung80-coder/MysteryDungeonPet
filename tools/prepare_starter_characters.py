"""원작 스타팅 허용 목록·몸체·감정 초상화를 재현 가능한 선택 패키지로 만든다."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from tools.prepare_party_actor import prepare as prepare_body
from tools.prepare_dialogue_portraits import prepare as prepare_portraits, body_identity

NAMES = {1:'이상해씨',4:'파이리',7:'꼬부기',25:'피카츄',37:'식스테일',52:'나옹',133:'이브이',152:'치코리타',155:'브케인',158:'리아코',231:'코코리',252:'나무지기',255:'아차모',258:'물짱이',300:'에나비',387:'모부기',390:'불꽃숭이',393:'팽도리',403:'꼬링크',446:'먹고자',447:'리오르'}

def prepare(root, output):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.util import get_ppmdu_config_for_rom, get_binary_from_rom
    from skytemple_files.hardcoded.personality_test_starters import HardcodedPersonalityTestStarters as Starters
    if output.exists():
        raise ValueError('choose a new output directory')
    source=root/'assets/original/source/user-supplied.nds'
    digest=lambda b:hashlib.sha256(b).hexdigest()
    before=digest(source.read_bytes());rom=NintendoDSRom.fromFile(str(source));config=get_ppmdu_config_for_rom(rom)
    overlay=get_binary_from_rom(rom,config.bin_sections.overlay13)
    heroes=set(map(int,Starters.get_player_md_ids(overlay,config)));partners=set(map(int,Starters.get_partner_md_ids(overlay,config)))
    md=FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md'))
    if len({md.entries[i].national_pokedex_number for i in heroes})!=19 or len(partners)!=21:
        raise ValueError('unexpected original starter list')
    output.mkdir(parents=True);characters=[];bodies={};entries=[]
    for index in sorted(heroes|partners):
        record=md.entries[index];sprite=int(record.sprite_index)
        if sprite not in bodies:
            dest=output/'bodies'/str(sprite)
            prepare_body(root,dest,index,scene_actor=True)
            bodies[sprite]=dest
        dest=bodies[sprite];src,key,body_hash,_=body_identity(dest/'asset.mdpa')
        identity=f'rom:{before}:{config.game_edition}:md:{index}'
        name=NAMES[int(record.national_pokedex_number)]
        entry={'mdIndex':index,'roleMask':(1 if index in heroes else 0)|(2 if index in partners else 0),
               'characterId':identity,'bodyAssetKey':key,'bodyAtlasSha256':body_hash,
               'bodyManifest':f'starters/bodies/{sprite}/asset.mdpa','displayName':name,'portraitClip':index}
        entries.append(entry)
        characters.append({'mdIndex':index,'kaoIndex':index%600-1,'displayName':name})
    characters.extend([{'mdIndex':383,'kaoIndex':382,'displayName':'켈리몬'}, {'mdIndex':322,'kaoIndex':321,'displayName':'노공룡'}])
    catalogue={'schemaVersion':1,'romSha256':before,'edition':config.game_edition,'characters':characters}
    character_file=output/'characters.json';character_file.write_text(json.dumps(catalogue,ensure_ascii=False,indent=2)+'\n')
    prepare_portraits(root,output/'portraits',characters_path=character_file,expressions=True)
    body=bytearray(struct.pack('<2I',0x4353444D,1))
    def u32(v):body.extend(struct.pack('<I',v))
    def string(v):
        raw=v.encode('utf-8');u32(len(raw));body.extend(raw)
    string(before);u32(len(entries))
    for entry in entries:
        u32(entry['mdIndex']);u32(entry['roleMask'])
        for field in ['characterId','bodyAssetKey','bodyAtlasSha256','bodyManifest','displayName']:string(entry[field])
    (output/'starters.mdsc').write_bytes(body)
    (output/'catalogue.json').write_text(json.dumps({'romSha256':before,'heroSlots':sorted(heroes),'partnerSlots':sorted(partners),'characters':entries},ensure_ascii=False,indent=2)+'\n')
    (output/'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original ROM starter bodies and portraits\n')
    if before!=digest(source.read_bytes()):raise ValueError('ROM changed')
    return {'romUnchanged':True,'characters':len(entries),'bodyPackages':len(bodies),'heroSpecies':19,'partnerSpecies':21}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();print(json.dumps(prepare(args.root,args.out)))
