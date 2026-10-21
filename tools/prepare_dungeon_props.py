"""Extract the confirmed original dungeon props; never modify ROM or source archives."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.mdpet.common import file_hash,write_json
from tools.package_format import encode_package

def prepare(root,output):
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.util import get_ppmdu_config_for_rom
    from PIL import Image
    if output.exists(): raise ValueError('choose a new output directory')
    source=root/'assets/original/source/user-supplied.nds';before=file_hash(source)
    rom=NintendoDSRom.fromFile(str(source));raw=rom.getFileByName('DUNGEON/dungeon.bin')
    pack=FileType.DUNGEON_BIN.deserialize(raw,get_ppmdu_config_for_rom(rom))
    images=[];provenance=[]
    for slot,sprite,palette,name,anchor in [(1022,2,0,'OranBerry',[8,8]),(1033,27,1,'StairsDown',[12,12]),(1033,28,1,'StairsUp',[12,12])]:
        indexed=pack[slot].to_pil(sprite,palette);image=indexed.convert('RGBA')
        image.putalpha(Image.frombytes('L',image.size,bytes(0 if i%16==0 else 255 for i in indexed.tobytes())))
        images.append((name,image,anchor));provenance.append({'slot':slot,'sprite':sprite,'palette':palette,'size':image.size,'rgbaSha256':hashlib.sha256(image.tobytes()).hexdigest()})
    data=pack[995];count=struct.unpack_from('<I',data)[0]
    if count!=50 or len(data)!=4+count*32:raise ValueError('shadow tile table mismatch')
    disc=Image.new('RGBA',(16,8));palette=pack[997].palettes[0]
    for tile in range(2):
        pixels=[i for b in data[4+(8+tile)*32:4+(9+tile)*32] for i in (b&15,b>>4)]
        for n,index in enumerate(pixels):disc.putpixel((tile*8+n%8,n//8),tuple(palette[index*3:index*3+3])+(255 if index else 0,))
    images.append(('YellowDisc',disc,[8,4]));provenance.append({'slot':995,'tiles':[8,9],'paletteSlot':997,'palette':0,'shadowSize':1,'size':[16,8],'rgbaSha256':hashlib.sha256(disc.tobytes()).hexdigest(),'policy':'player and all friendly NPCs; user policy'})
    item=FileType.ITEM_P.deserialize(rom.getFileByName('BALANCE/item_p.bin')).item_list[109]
    indexed=pack[1022].to_pil(item.sprite,item.palette);apple=indexed.convert('RGBA')
    apple.putalpha(Image.frombytes('L',apple.size,bytes(0 if i%16==0 else 255 for i in indexed.tobytes())))
    images.append(('Apple',apple,[8,8]));provenance.append({'itemId':109,'slot':1022,'sprite':item.sprite,'palette':item.palette,'size':apple.size,'rgbaSha256':hashlib.sha256(apple.tobytes()).hexdigest(),'policy':'Wigglytuff head prop during communal meal; reuses original item graphic'})
    for sprite,name in ((6,'WarpTrap'),(30,'ShopStand')):
        indexed=pack[1033].to_pil(sprite,1);image=indexed.convert('RGBA')
        image.putalpha(Image.frombytes('L',image.size,bytes(0 if i%16==0 else 255 for i in indexed.tobytes())))
        images.append((name,image,[12,12]));provenance.append({'slot':1033,'sprite':sprite,'palette':1,'size':image.size,'rgbaSha256':hashlib.sha256(image.tobytes()).hexdigest(),'policy':'explicit retained trap palette map; no guessed warp effect ID'})
    with tempfile.TemporaryDirectory() as temp:
        out=Path(temp);atlas=Image.new('RGBA',(24,sum(image.height for _,image,_ in images)));clips=[];y=0
        for i,(name,image,anchor) in enumerate(images):
            atlas.paste(image,(0,y));clips.append({'id':i,'name':name,'rows':[[{'rect':[0,y,*image.size],'anchor':anchor,'shadow':[0,0],'ticks':1,'markers':[]}]]});y+=image.height
        atlas.save(out/'atlas.png')
        package={'assetVersion':1,'tickHz':60,'width':24,'height':atlas.height,'pngBytes':(out/'atlas.png').stat().st_size,'sourceKind':1,'atlasName':'atlas.png','pngSha256':file_hash(out/'atlas.png'),'assetKey':'rom:EoS_NA:dungeon-props','provenance':json.dumps(provenance),'clips':clips}
        (out/'props.mdpa').write_bytes(encode_package(package));(out/'CREDITS.txt').write_text('Nintendo / Pokémon / CHUNSOFT; original dungeon graphics\n',encoding='utf-8')
        report={'romHashUnchanged':file_hash(source)==before,'romSha256':before,'sourcePath':'DUNGEON/dungeon.bin','sourceSha256':hashlib.sha256(raw).hexdigest(),'props':provenance}
        if not report['romHashUnchanged']:raise ValueError('ROM changed')
        write_json(out/'provenance.json',report);output.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(out,output)
    return report
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(json.dumps(prepare(a.root,a.out),ensure_ascii=False,indent=2))
