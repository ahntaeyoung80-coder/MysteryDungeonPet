"""조사용 쉼터 정적 표본. 원작 BMA→DMA 규칙, 타일 변형0, 정지 캥카 배우만 합성한다.
원작 스크린샷/동적 팔레트/다른 배우·오브젝트·연출을 재현한 결과는 아니다.
"""
from pathlib import Path
import sys,json,hashlib
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT))
OUT=Path(__file__).resolve().parent
from ndspy.rom import NintendoDSRom
from skytemple_files.common.util import get_ppmdu_config_for_rom,get_binary_from_rom
from skytemple_files.common.types.file_types import FileType
from skytemple_files.hardcoded.ground_dungeon_tilesets import HardcodedGroundDungeonTilesets,resolve_mapping_for_level
from skytemple_files.hardcoded.dungeons import HardcodedDungeons
from skytemple_files.graphics.dma.dma_drawer import DmaDrawer
from skytemple_files.graphics.chara_wan.handler import CharaWanHandler
from tools.mdpet.extractor import checked_pack
from tools.mdpet.wan_verify import render_wan_frame
sha=lambda b:hashlib.sha256(b).hexdigest()
def run():
 source=ROOT/'assets/original/source/user-supplied.nds';before=sha(source.read_bytes());rom=NintendoDSRom.fromFile(str(source));c=get_ppmdu_config_for_rom(rom)
 mappings=HardcodedGroundDungeonTilesets.get_ground_dungeon_tilesets(get_binary_from_rom(rom,c.bin_sections.overlay11),c)
 definitions=HardcodedDungeons.get_dungeon_list(get_binary_from_rom(rom,c.bin_sections.arm9),c)
 mappa=FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'));fixed=FileType.FIXED_BIN.deserialize(rom.getFileByName('BALANCE/fixed.bin'))
 dbin=FileType.DUNGEON_BIN.deserialize(rom.getFileByName('DUNGEON/dungeon.bin'),c)
 md=FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md')).entries[554];raw=checked_pack(rom.getFileByName('MONSTER/m_ground.bin'))[md.sprite_index]
 wan=CharaWanHandler.deserialize(raw);statue=render_wan_frame(wan,wan.animGroupData[0][0][0],(64,64));statue.save(OUT/'kangaskhan-statue.png')
 bglist=FileType.BG_LIST_DAT.deserialize(rom.getFileByName('MAP_BG/bg_list.dat'));pictures=[];proof=[]
 grounds=json.loads((OUT/'grounds.json').read_text())
 for g in grounds:
  level=c.script_data.level_list__by_name[g['level']];bg=bglist.level[level.mapid]
  def load(kind,name,ext):return kind.deserialize(rom.getFileByName(f'MAP_BG/{name.lower()}.{ext}'))
  bma=load(FileType.BMA,bg.bma_name,'bma');mapped=resolve_mapping_for_level(level,mappings,mappa,fixed,dbin,definitions)
  if mapped:
   dma,dpc,dpci,dpl,_,fixedroom=mapped;drawer=DmaDrawer(dma)
   rules=drawer.rules_from_fixed_room(fixedroom) if fixedroom else drawer.rules_from_bma(bma.layer0,bma.map_width_chunks)
   chunks=drawer.get_mappings_for_rules(rules,treat_outside_as_wall=True,variation_index=0)
   pic=drawer.draw(chunks,dpci,dpc,dpl,None)[0].convert('RGBA')
  else:
   bpc=load(FileType.BPC,bg.bpc_name,'bpc');bpl=load(FileType.BPL,bg.bpl_name,'bpl');bpas=[load(FileType.BPA,n,'bpa') if n else None for n in bg.bpa_names]
   original=bpc.chunks_animated_to_pil
   def materialize(*a,**kw):
    images=original(*a,**kw)
    for ch in images:
     if ch.palette and not isinstance(ch.palette.palette,(bytes,bytearray,list)):ch.palette.palette=bytes(ch.palette.palette)
    return images
   bpc.chunks_animated_to_pil=materialize
   pic=bma.to_pil(bpc,bpl,bpas,include_collision=False,include_unknown_data_block=False,pal_ani=False,single_frame=True)[0].convert('RGBA')
  pic.putalpha(255)
  for a in g['statueLayout']['actors'] if g['statueLayout'] else []:
   p=a['position'];x=p['xRelative']*8+(4 if p['xOffset']>=2 else 0);y=p['yRelative']*8+(4 if p['yOffset']>=2 else 0)
   pic.alpha_composite(statue,(x-32,y-36))
  if g['level']=='D30P21A':pic.save(OUT/'temporal-tower-midpoint.png')
  proof.append({'groundId':g['id'],'size':list(pic.size),'rgbaSha256':sha(pic.tobytes()),'mode':'dungeon_tiles' if mapped else 'static_background'})
  cell=Image.new('RGB',(240,224),(25,28,33));small=pic.convert('RGB');small.thumbnail((232,196),Image.Resampling.NEAREST)
  cell.paste(small,((240-small.width)//2,24));ImageDraw.Draw(cell).text((8,6),f"L{level.id:03d} {g['level']}",fill='white');pictures.append(cell)
 for page in range(0,len(pictures),24):
  batch=pictures[page:page+24];sheet=Image.new('RGB',(1440,224*((len(batch)+5)//6)),(25,28,33))
  for i,pic in enumerate(batch):sheet.paste(pic,((i%6)*240,(i//6)*224))
  sheet.save(OUT/f'connectors-{page//24+1}.png')
 assert sha(source.read_bytes())==before
 (OUT/'preview-evidence.json').write_text(json.dumps({'romSha256':before,'romUnchanged':True,'dungeonBinSha256':sha(rom.getFileByName('DUNGEON/dungeon.bin')),
   'statue':{'mdIndex':554,'spriteIndex':int(md.sprite_index),'pack':'MONSTER/m_ground.bin','wanSha256':sha(raw)},'grounds':proof},indent=2)+'\n')
 print('previews',len(proof))
if __name__=='__main__':run()
