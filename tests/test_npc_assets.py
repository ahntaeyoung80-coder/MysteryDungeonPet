"""선택 NPC의 실제 ROM 종·몸체 출처·KAO 픽셀과 감정 대체를 독립 대조한다."""
import hashlib
import json
from pathlib import Path
import unittest
from tools.prepare_dialogue_portraits import body_identity

ROOT = Path(__file__).resolve().parents[1]
GAMEPLAY = ROOT/'assets/original/gameplay'


class NpcAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from ndspy.rom import NintendoDSRom
        from skytemple_files.common.types.file_types import FileType
        from PIL import Image
        cls.rom = NintendoDSRom.fromFile(str(ROOT/'assets/original/source/user-supplied.nds'))
        cls.md = FileType.MD.deserialize(cls.rom.getFileByName('BALANCE/monster.md'))
        cls.kao = FileType.KAO.deserialize(cls.rom.getFileByName('FONT/kaomado.kao'))
        cls.talk = FileType.TBL_TALK.deserialize(cls.rom.getFileByName('MESSAGE/tbl_talk.tlk'))
        cls.catalogue = json.loads((GAMEPLAY/'npcs/catalogue.json').read_text())
        cls.audit = json.loads((GAMEPLAY/'portraits/provenance.json').read_text())['portraits']
        cls.atlas = Image.open(GAMEPLAY/'portraits/atlas.png').convert('RGBA')
        cls.columns = sorted({a['bodyMdIndex'] for a in cls.audit})

    def test_selected_md_body_sources_and_original_talk_groups(self):
        expected={16,83,102,126,191,193,237,240,293,341,342,369,383,384,425,434,437,25,85,125,128,172,180,181,230,337,438,439,440,511,352}
        self.assertEqual({c['mdIndex'] for c in self.catalogue['characters']},expected)
        for c in self.catalogue['characters']:
            record=self.md.entries[c['mdIndex']]
            self.assertEqual(c['speciesId'],record.national_pokedex_number)
            self.assertEqual(c['spriteIndex'],record.sprite_index)
            self.assertEqual(c['talkGroup'],self.talk.get_monster_personality(c['mdIndex']))
            source,key,body_hash,_=body_identity(GAMEPLAY/c['bodyManifest'])
            self.assertEqual(source['spriteIndex'],record.sprite_index)
            self.assertEqual(source['nationalDex'],record.national_pokedex_number)
            self.assertEqual(source['romHash'],self.catalogue['romSha256'])
            self.assertEqual(c['bodyAssetKey'],key)
            self.assertEqual(c['bodyAtlasSha256'],body_hash)
            self.assertEqual(hashlib.sha256((GAMEPLAY/Path(c['bodyManifest']).parent/'atlas.png').read_bytes()).hexdigest(),body_hash)
        granbull=next(c for c in self.catalogue['characters'] if c['mdIndex']==237)
        self.assertEqual((granbull['speciesId'],granbull['spriteIndex'],granbull['kaoIndex']),(210,257,236))

    def test_each_original_face_or_same_character_normal_pixels(self):
        for c in self.catalogue['characters']:
            mask=sum(1<<m for m in range(14) if self.kao.get(c['kaoIndex'],m*2))
            self.assertEqual(c['originalEmotionMask'],mask)
            self.assertEqual(c['emotionMask'],(1<<14)-1 if c['mdIndex']==383 else mask)
            self.assertTrue(mask&1)
            for mood in range(14):
                original=self.kao.get(c['kaoIndex'],mood*2)
                actual=mood if original else 0
                # Existing explicitly credited Kecleon supplements are retained; all new NPCs use ROM only.
                audit=next(a for a in self.audit if a['bodyMdIndex']==c['mdIndex'] and a['expression']==mood)
                if isinstance(audit['portraitSource'],dict):
                    self.assertEqual(c['mdIndex'],383)
                    continue
                self.assertEqual(audit['actualExpression'],actual)
                self.assertEqual(audit['kaoIndex'],c['kaoIndex'])
                expected=self.kao.get(c['kaoIndex'],actual*2).get().convert('RGBA').tobytes()
                x=self.columns.index(c['mdIndex'])*40
                image=self.atlas.crop((x,mood*40,x+40,mood*40+40))
                self.assertEqual(image.tobytes(),expected,(c['mdIndex'],mood))

    def test_new_wan_audits_and_existing_body_reuse(self):
        new=[c for c in self.catalogue['characters'] if c['verification']]
        self.assertEqual(len(new),26)
        self.assertEqual(sum(c['verification']['comparedFrames'] for c in new),5378)
        for c in new:
            proof=json.loads((GAMEPLAY/Path(c['bodyManifest']).parent/'provenance.json').read_text())
            self.assertEqual(proof['verification']['status'],'pass')
            self.assertGreater(proof['verification']['originalSourceFrames'],0)
            for key,value in proof['verification'].items():
                self.assertEqual(c['verification'][key],value)
            # Missing Nod/Faint are explicit, rather than copied/renamed unrelated clips.
            for clip in proof['missingSceneClips']:
                self.assertIn(clip,('22','33'))
        reused={c['mdIndex']:c['bodyManifest'] for c in self.catalogue['characters'] if not c['verification']}
        self.assertEqual(set(reused),{25,383,384,425,438})
        self.assertEqual({md:reused[md] for md in (383,384,425)},{383:'actors/asset-2/asset.mdpa',384:'actors/asset-3/asset.mdpa',425:'starters/bodies/305/asset.mdpa'})
        visitors={c['visitorActorId']:c['mdIndex'] for c in self.catalogue['characters'] if c.get('visitorActorId')}
        self.assertEqual(visitors,{113:352,115:369,124:434})


if __name__=='__main__':
    unittest.main()
