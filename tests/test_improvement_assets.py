"""Original full-map pixels and confirmed prop extraction, independent of native draw timing."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image
from ndspy.rom import NintendoDSRom
from skytemple_files.common.types.file_types import FileType
from tools.prepare_dungeon_props import prepare

ROOT = Path(__file__).resolve().parents[1]

class ImprovementAssets(unittest.TestCase):
    def test_confirmed_props_reproduce_committed_files(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'props'
            report=prepare(ROOT,output)
            self.assertTrue(report['romHashUnchanged'])
            for original in (ROOT/'assets/original/gameplay/props').iterdir():
                self.assertEqual(original.read_bytes(),(output/original.name).read_bytes(),original.name)

    def test_full_backgrounds_preserve_original_pixels_and_window_frame(self):
        rom=NintendoDSRom.fromFile(str(ROOT/'assets/original/source/user-supplied.nds'))
        backgrounds=FileType.BG_LIST_DAT.deserialize(rom.getFileByName('MAP_BG/bg_list.dat'))
        maps=json.loads((ROOT/'assets/original/gameplay/provenance.json').read_text())['home']
        with Image.open(ROOT/'assets/original/gameplay/home/atlas.png') as image:
            atlas=image.convert('RGBA')
        for info in maps:
            entry=next(e for e in backgrounds.level if e.bma_name==info['name'])
            def load(kind,name,extension):
                return kind.deserialize(rom.getFileByName(f'MAP_BG/{name.lower()}.{extension}'))
            bma=load(FileType.BMA,entry.bma_name,'bma')
            bpc=load(FileType.BPC,entry.bpc_name,'bpc')
            bpl=load(FileType.BPL,entry.bpl_name,'bpl')
            bpas=[load(FileType.BPA,n,'bpa') if n else None for n in entry.bpa_names]
            expected=bma.to_pil(bpc,bpl,bpas,include_collision=False,
                include_unknown_data_block=False,pal_ani=False,single_frame=True)[0].convert('RGBA')
            expected.putalpha(255)
            changed=0
            if info['name']=='G01P07C':
                for y in range(96,133):
                    for x in range(148,190):
                        if expected.getpixel((x,y))[:3]==(7,31,95):
                            expected.putpixel((x,y),(0,0,0,255));changed+=1
                self.assertEqual(changed,306)
            x,y,width,height=info['rect']
            self.assertEqual(expected.size,(width,height))
            self.assertEqual(expected.tobytes(),atlas.crop((x,y,x+width,y+height)).tobytes(),info['name'])
        self.assertEqual(maps[0]['sourceSize'],[504,360])
        self.assertEqual(hashlib.sha256((ROOT/'assets/original/source/user-supplied.nds').read_bytes()).hexdigest(),
            '6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf')
