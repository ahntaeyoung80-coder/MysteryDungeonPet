from __future__ import annotations

import copy
import io
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace as NS
import unittest
from contextlib import redirect_stderr, redirect_stdout

from tools.mdpet.__main__ import main
from tools.mdpet.catalogue import cdf_weights, floor_index, item_distribution, monster_distributions
from tools.mdpet.common import file_hash, read_json, staging, write_json
from tools.mdpet.extractor import checked_pack, raw_wan_metadata, select_floors
from tools.mdpet.fixture import create_sheets, provenance
from tools.mdpet.packer import pack_asset, validate_package, write_package


class Distributions(unittest.TestCase):
    def test_cdf_cases(self):
        for source, expected in (([], []), ([0, 0], [0, 0]), ([100, 0, 300, 1000], [1, 0, 2, 7]),
                                 ([100, 100, 300], [1, 0, 2]), ([40], [1])):
            with self.subTest(source=source):
                self.assertEqual(cdf_weights(source), expected)
        for bad in ([3, 2], [-1], [65535], [True], [1.5]):
            with self.assertRaises(ValueError):
                cdf_weights(bad)

    def test_group_floor_first_and_last(self):
        d = NS(number_floors=3, mappa_index=1, start_after=2)
        groups = [[], [object() for _ in range(5)]]
        self.assertEqual(floor_index(d, 0, groups), 2)
        self.assertEqual(floor_index(d, 2, groups), 4)
        for bad in (-1, 3):
            with self.assertRaises(ValueError):
                floor_index(d, bad, groups)
        with self.assertRaises(ValueError):
            floor_index(d, 2, [[], [None] * 4])
        defs = [NS(number_floors=1, mappa_index=0, start_after=0), d]
        self.assertEqual(select_floors(defs, [[None], groups[1]], []), [(0, 0), (1, 0), (1, 2)])
        with self.assertRaises(ValueError):
            select_floors(defs, [[None], groups[1]], [(180, 0)])

    def test_item_category_cdf_and_guaranteed(self):
        items = [{"itemId": 100 + i, "categoryId": 9} for i in range(6)]
        data = NS(categories={0: 100, 1: 300}, items={0: 100, 1: 300, 2: 65535, 3: 200, 4: 0, 5: 800})
        # ROM category member list가 item_p.category와 다를 때에도 원본 GUI 규칙을 사용한다.
        result = item_distribution(data, items, "floor", {0: [0, 1, 2], 1: [3, 4, 5]})
        self.assertEqual([e["relativeWeight"] for e in result["categories"]["entries"]], [1, 2])
        self.assertEqual([[e["relativeWeight"] for e in c["entries"]] for c in result["conditionalItems"]], [[1, 2], [1, 0, 3]])
        self.assertEqual(result["guaranteed"], [{"recordIndex": 2, "itemId": 102, "categoryId": 0, "rawCDF": 65535}])
        data.items[1] = 50
        with self.assertRaises(ValueError):
            item_distribution(data, items, "floor", {0: [0, 1, 2], 1: [3, 4, 5]})
        with self.assertRaises(ValueError):
            item_distribution(NS(categories={}, items={8: 1}), items, "floor")

    def test_monster_contexts_and_special_rows(self):
        rows = [NS(md_index=i, level=2, main_spawn_weight=n, monster_house_spawn_weight=m)
                for i, n, m in ((1, 100, 20), (0x229, 200, 40), (2, 400, 100), (383, 0, 0))]
        result = monster_distributions(rows, 1000)
        self.assertEqual([e["relativeWeight"] for e in result["normal"]["entries"]], [1, 2])
        self.assertEqual([e["relativeWeight"] for e in result["monsterHouse"]["entries"]], [1, 3])
        self.assertEqual([e["mdIndex"] for e in result["specialRows"]], [0x229, 383])
        with self.assertRaises(ValueError):
            monster_distributions(rows, 10)

    def test_bin_pack_rejects_truncation_and_offsets(self):
        for data in (b"", b"1234567", struct.pack("<II", 0, 100),
                     struct.pack("<IIII", 0, 1, 16, 20), struct.pack("<IIII", 0, 1, 0, 0)):
            with self.assertRaises(ValueError):
                checked_pack(data)


class Packages(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.sheets = self.root / "sheets"
        create_sheets(self.sheets)
        self.raw = read_json(self.sheets / "raw-wan.json")

    def tearDown(self):
        self.temp.cleanup()

    def pack(self, name="package"):
        root = self.root / name
        root.mkdir()
        asset, support = pack_asset(self.sheets, root, "test:shapes", provenance(), self.raw)
        write_package(root, [asset], [support])
        return root

    def test_pixels_markers_alias_rows_ticks_and_reproducibility(self):
        first = self.pack("first")
        second = self.pack("second")
        self.assertEqual(file_hash(first / "manifest.json"), file_hash(second / "manifest.json"))
        self.assertEqual(file_hash(first / "atlas.png"), file_hash(second / "atlas.png"))
        asset = read_json(first / "manifest.json")["assets"][0]
        clips = {a["name"]: a for a in asset["animations"]}
        self.assertEqual(clips["Sleep"]["rows"], 1)
        self.assertEqual(clips["Eat"]["rows"], 1)
        self.assertEqual(clips["Walk"]["rows"], 8)
        self.assertEqual(len(clips["Walk"]["directions"][7]), 2)
        self.assertEqual(clips["Walk"]["directions"][6][0]["durationTicks"], 14)
        self.assertEqual(clips["Walk"]["directions"][0][0]["points"]["lhand"],
                         clips["Walk"]["directions"][0][0]["points"]["rhand"])
        self.assertEqual(clips["Idle"]["wanId"], 7)
        self.assertEqual(clips["Idle"]["aliasOf"], "Walk")
        self.assertEqual(clips["Idle"]["rawDirections"], self.raw["7"])
        self.assertEqual(validate_package(first)["frameCount"], 75)
        self.assertTrue(read_json(first / "support.json")["assets"][0]["requiredComplete"])
        self.assertIsNone(asset["source"]["mdIndex"])

    def test_mutated_package_rejected(self):
        root = self.pack()
        original = read_json(root / "manifest.json")
        for case in ("version", "assetVersion", "rect", "duration", "aliasCycle", "aliasMissing", "atlasMissing",
                     "pathTraversal", "negativeRect", "anchor", "pixelHash", "fakeRom"):
            with self.subTest(case=case):
                manifest = copy.deepcopy(original)
                asset = manifest["assets"][0]
                frame = next(c for c in asset["animations"] if c["aliasOf"] is None)["directions"][0][0]
                idle = next(c for c in asset["animations"] if c["name"] == "Idle")
                if case == "version": manifest["schemaVersion"] = 999
                if case == "assetVersion": manifest["assetVersion"] = 999
                if case == "rect": frame["rect"][0] = asset["atlas"]["width"]
                if case == "duration": frame["durationTicks"] = 0
                if case == "aliasCycle": idle["aliasOf"] = "EventSleep"
                if case == "aliasMissing": idle["aliasOf"] = "Missing"
                if case == "atlasMissing": asset["atlas"]["file"] = "missing.png"
                if case == "pathTraversal": asset["atlas"]["file"] = "../first/atlas.png"
                if case == "negativeRect": frame["rect"][0] = -1
                if case == "anchor": frame["anchor"][0] += 1
                if case == "pixelHash": frame["sourceRGBAHash"] = "0" * 64
                if case == "fakeRom": asset["source"]["mdIndex"] = 25
                write_json(root / "manifest.json", manifest)
                with self.assertRaises((ValueError, FileNotFoundError)):
                    validate_package(root)
        write_json(root / "manifest.json", original)
        (root / "atlas.png").write_bytes((root / "atlas.png").read_bytes()[:24])
        with self.assertRaises(ValueError):
            validate_package(root)

    def test_alias_cycle_and_missing_sheet_rejected_before_pack(self):
        import xml.etree.ElementTree as ET
        path = self.sheets / "AnimData.xml"
        xml = ET.parse(path)
        for node in xml.findall("Anims/Anim"):
            if node.findtext("Name") == "Idle":
                node.find("CopyOf").text = "EventSleep"
        xml.write(path)
        with self.assertRaises(ValueError):
            self.pack()

    def test_failure_preserves_published_output(self):
        root = self.pack()
        before = file_hash(root / "manifest.json")
        with self.assertRaises(FileExistsError):
            with staging(root):
                self.fail("existing package must not be opened for writing")
        self.assertEqual(file_hash(root / "manifest.json"), before)
        failed = self.root / "failed"
        with self.assertRaises(ValueError):
            with staging(failed) as temp:
                (temp / "partial").write_text("partial")
                raise ValueError("injected conversion failure")
        self.assertFalse(failed.exists())
        self.assertFalse(list(self.root.glob(".mdpet-*")))

    def test_cli_unsupported_rom_retains_hash_and_failure_stage(self):
        from ndspy.rom import NintendoDSRom
        rom = NintendoDSRom()
        rom.idCode = b"TEST"
        rom.arm9 = bytes(32)
        path = self.root / "unsupported.nds"
        rom.saveToFile(str(path))
        before = file_hash(path)
        out = self.root / "rom-out"
        with redirect_stderr(io.StringIO()), redirect_stdout(io.StringIO()):
            code = main(["extract", "--rom", str(path), "--out", str(out)])
        self.assertEqual(code, 1)
        self.assertFalse(out.exists())
        report = read_json(self.root / "rom-out.failure.json")
        self.assertEqual(report["stage"], "extract")
        self.assertEqual(report["context"]["function"], "get_ppmdu_config_for_rom")
        self.assertEqual(report["context"]["reports"]["identity"]["gameCode"], "TEST")
        self.assertEqual(report["context"]["reports"]["errors"]["romHashBefore"], before)
        self.assertEqual(file_hash(path), before)


if __name__ == "__main__":
    unittest.main()
