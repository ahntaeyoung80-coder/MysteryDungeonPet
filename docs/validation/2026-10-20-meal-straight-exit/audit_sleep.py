import json,subprocess,hashlib
from pathlib import Path
from PIL import Image
from ndspy.rom import NintendoDSRom
from skytemple_files.common.types.file_types import FileType
from skytemple_files.graphics.chara_wan.handler import CharaWanHandler
from tools.mdpet.extractor import checked_pack
from tools.mdpet.wan_verify import render_wan_frame
rom=NintendoDSRom.fromFile('assets/original/source/user-supplied.nds')
md=FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md'))
pack=checked_pack(rom.getFileByName('MONSTER/m_ground.bin'));results=[]
for asset,index in [('asset-0',1),('asset-1',4)]:
 path=Path('assets/original/gameplay/actors')/asset/'asset.mdpa'
 package=json.loads(subprocess.check_output(['build/linux/pet_package_inspect',str(path)]))
 atlas=Image.open(path.parent/'atlas.png').convert('RGBA');wan=CharaWanHandler.deserialize(pack[md.entries[index].sprite_index])
 for clipid in [27,13]:
  clip=next(c for c in package['clips'] if c['id']==clipid)
  metadata=json.loads(bytes.fromhex(clip['sourceMetadataHex']))
  shape=metadata['frameSize'];matched=0
  for direction,sequence in enumerate(wan.animGroupData[clipid]):
   retained=clip['rows'][direction]
   assert len(retained)==len(sequence),(asset,clipid,direction)
   for source,frame in zip(sequence,retained):
    assert frame['ticks']==source.duration
    original=render_wan_frame(wan,source,shape)
    x,y,w,h=frame['rect'];canvas=Image.new('RGBA',tuple(shape));canvas.paste(atlas.crop((x,y,x+w,y+h)),tuple(frame['cropOrigin']))
    assert canvas.tobytes()==original.tobytes(),(asset,clipid,direction)
    matched+=1
  results.append({'asset':asset,'mdIndex':index,'clip':clipid,'directions':len(clip['rows']),'sourceFramesPerDirection':len(wan.animGroupData[clipid][0]),'retainedFramesPerDirection':len(clip['rows'][0]),'ticks':[f['ticks'] for f in clip['rows'][0]],'matchedOriginalFrames':matched,'status':'pass'})
print(json.dumps({'source':'ROM MONSTER/m_ground.bin','result':'Laying27 has one original pose; EventSleep13 has two original poses. No discarded intermediate frames in these sequences.','clips':results},indent=2))
