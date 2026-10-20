"""실제 원본 종/그림을 대조해 잘못된 수동 초상화 매핑이 재발하지 않게 한다."""
import hashlib
import json
from pathlib import Path
import unittest
from tools.prepare_dialogue_portraits import body_identity

ROOT = Path(__file__).resolve().parents[1]
GAMEPLAY = ROOT / 'assets/original/gameplay'

class CharacterAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from ndspy.rom import NintendoDSRom
        from skytemple_files.common.types.file_types import FileType
        from PIL import Image
        cls.rom = NintendoDSRom.fromFile(str(ROOT/'assets/original/source/user-supplied.nds'))
        cls.md = FileType.MD.deserialize(cls.rom.getFileByName('BALANCE/monster.md'))
        cls.kao = FileType.KAO.deserialize(cls.rom.getFileByName('FONT/kaomado.kao'))
        cls.atlas = Image.open(GAMEPLAY/'portraits/atlas.png').convert('RGBA')
        cls.audit = json.loads((GAMEPLAY/'portraits/provenance.json').read_text())['portraits']
        cls.columns = sorted({p['bodyMdIndex'] for p in cls.audit})

    def face(self, md, expression=0):
        left = self.columns.index(md)*40
        return self.atlas.crop((left, expression*40, left+40, expression*40+40))

    def test_exact_npc_species_pixels_not_only_self_consistent_metadata(self):
        expected = self.kao.get(382, 0).get().convert('RGBA').tobytes()
        wrong = self.kao.get(373, 0).get().convert('RGBA').tobytes()
        self.assertEqual(self.md.entries[383].national_pokedex_number, 352)
        self.assertEqual(self.md.entries[374].national_pokedex_number, 346)
        self.assertEqual(self.face(383).tobytes(), expected)
        self.assertNotEqual(self.face(383).tobytes(), wrong)
        self.assertEqual(self.face(322).tobytes(), self.kao.get(321, 0).get().convert('RGBA').tobytes())

    def test_every_starter_body_gender_id_and_fourteen_original_faces(self):
        catalogue = json.loads((GAMEPLAY/'starters/catalogue.json').read_text())
        self.assertEqual(len(catalogue['characters']), 34)
        for character in catalogue['characters']:
            md = character['mdIndex']
            self.assertEqual(character['speciesId'], self.md.entries[md].national_pokedex_number, md)
            source, _, _, identity = body_identity(GAMEPLAY/character['bodyManifest'])
            self.assertEqual(source['spriteIndex'], self.md.entries[md].sprite_index, md)
            self.assertEqual(source['nationalDex'], self.md.entries[md].national_pokedex_number, md)
            self.assertEqual(character['characterId'], f"rom:{catalogue['romSha256']}:EoS_NA:md:{md}")
            self.assertTrue(identity.startswith('rom:'+catalogue['romSha256']))
            for mood in range(14):
                original = self.kao.get(md % 600-1, mood*2)
                self.assertIsNotNone(original, (md, mood))
                self.assertEqual(self.face(md, mood).tobytes(), original.get().convert('RGBA').tobytes(), (md, mood))

    def test_product_starters_include_meowth_and_munchlax_in_both_roles(self):
        catalogue = json.loads((GAMEPLAY/'starters/catalogue.json').read_text())
        self.assertEqual(catalogue['schemaVersion'], 2)
        for role in (1, 2):
            species = {c['speciesId'] for c in catalogue['characters'] if c['roleMask'] & role}
            self.assertEqual(len(species), 21)
            self.assertTrue({52, 446}.issubset(species))
        # Keep original-ROM evidence separate from the product's union policy.
        self.assertNotIn(488, catalogue['heroSlots'])
        self.assertNotIn(52, catalogue['heroSlots'])
        self.assertTrue({52,488}.issubset(catalogue['playableSlots']))

    def test_npc_supplements_preserve_existing_original_expressions(self):
        from PIL import Image
        folder = ROOT/'assets/original/source/portrait-supplements'
        entries = json.loads((folder/'manifest.json').read_text())['files']
        for entry in entries:
            data = (folder/entry['file']).read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(), entry['sha256'])
            expected = Image.open(folder/entry['file']).convert('RGBA').tobytes()
            self.assertEqual(self.face(entry['mdIndex'], entry['expression']).tobytes(), expected)
        for md, kao, moods in [(383,382,[0,3,5]), (322,321,[0,1,2,3,10,12])]:
            for mood in moods:
                self.assertEqual(self.face(md,mood).tobytes(), self.kao.get(kao,mood*2).get().convert('RGBA').tobytes())

if __name__ == '__main__':
    unittest.main()
