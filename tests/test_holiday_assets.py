from copy import copy
import hashlib
import json
from pathlib import Path
import unittest
from PIL import Image
from ndspy.rom import NintendoDSRom
from skytemple_files.common.types.file_types import FileType
from skytemple_files.graphics.effect_wan.handler import EffectWanHandler
from skytemple_files.graphics.effect_wan.sheets import GenerateFrame
from tools.mdpet.extractor import checked_pack
from tools.prepare_dialogue_portraits import body_identity

ROOT=Path(__file__).resolve().parents[1]
HOLIDAY=ROOT/'assets/original/gameplay/holiday'


class HolidayAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rom=NintendoDSRom.fromFile(str(ROOT/'assets/original/source/user-supplied.nds'))
        cls.proof=json.loads((HOLIDAY/'provenance.json').read_text())

    def test_original_float_slot_sprite_and_verified_frames(self):
        md=FileType.MD.deserialize(self.rom.getFileByName('BALANCE/monster.md'))
        self.assertEqual(len(self.proof['floatBindings']),34)
        self.assertEqual(self.proof['floatComparedFrames'],704)
        for binding in self.proof['floatBindings']:
            source,key,body_hash,_=body_identity(HOLIDAY/binding['manifest'])
            self.assertEqual(source['spriteIndex'],md.entries[binding['mdIndex']].sprite_index)
            self.assertEqual(key,binding['bodyKey']);self.assertEqual(body_hash,binding['bodyHash'])
            self.assertEqual(binding['verification']['status'],'pass')

    def test_water_pixels_retain_original_front_back_palette_offset_and_tick(self):
        raw=checked_pack(self.rom.getFileByName('EFFECT/effect.bin'))[1]
        effect=EffectWanHandler.deserialize(raw);expected=[]
        for proof in self.proof['water']:
            self.assertEqual(proof['palette'],1);self.assertEqual(proof['ticks'],[12,12,12])
            for front in (False,True):
                for frame in effect.animData[proof['animation']]:
                    pieces=[]
                    for original in effect.frameData[frame.frmIndex]:
                        if original.front!=front:continue
                        self.assertEqual(original.paletteIndex,1)
                        p=copy(original);x,y=frame.offset;x=x-65536 if x>=32768 else x;y=y-65536 if y>=32768 else y
                        p.offset=(p.offset[0]+x,p.offset[1]+y);pieces.append(p)
                    expected.append(GenerateFrame(effect,pieces,tuple(proof['box'])))
        atlas=Image.open(HOLIDAY/'water.png').convert('RGBA');top=0
        for image in expected:
            self.assertEqual(atlas.crop((0,top,image.width,top+image.height)).tobytes(),image.tobytes());top+=image.height
        self.assertEqual(top,atlas.height)

    def test_ground_sources_pixels_and_routes_use_original_collision(self):
        for proof in self.proof['grounds']:
            for source in proof['sources']:
                self.assertEqual(hashlib.sha256(self.rom.getFileByName(source['path'])).hexdigest(),source['sha256'])
            image=Image.open(HOLIDAY/(proof['ground']+'.png')).convert('RGBA')
            self.assertEqual(hashlib.sha256(image.tobytes()).hexdigest(),proof['rgbaSha256'])
        for route in json.loads((HOLIDAY/'routes-proof.json').read_text()):
            bma=FileType.BMA.deserialize(self.rom.getFileByName('MAP_BG/'+route['scene'].lower()+'.bma'))
            for role in ('compiledPlayer','compiledPartner'):
                for x,y in route[role]:self.assertFalse(bma.collision[y//8*bma.map_width_camera+x//8])
