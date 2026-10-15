from __future__ import annotations

import importlib.metadata
from copy import deepcopy
import json
from pathlib import Path
from struct import unpack_from

from . import REQUIRED, SCHEMA_VERSION, SKYTEMPLE_COMMIT
from .catalogue import build_floor, floor_index, scalar
from .common import file_hash, sha256, write_json
from .packer import pack_asset, write_package


def smoke() -> dict:
    from skytemple_files.common.impl_cfg import ImplementationType, change_implementation_type
    change_implementation_type(ImplementationType.PYTHON)
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.ppmdu_config.xml_reader import Pmd2XmlReader
    from skytemple_files.hardcoded.dungeons import HardcodedDungeons
    from skytemple_files.dungeon_data.mappa_bin.validator.validator import DungeonValidator
    from skytemple_files.graphics.chara_wan.handler import CharaWanHandler
    config = Pmd2XmlReader.load_default()
    versions = {name: importlib.metadata.version(name) for name in
                ("skytemple-files", "skytemple-rust", "ndspy", "Pillow", "explorerscript",
                 "pmdsky-debug-py", "dungeon-eos", "range-typed-integers", "appdirs", "igraph", "texttable")}
    direct = json.loads(importlib.metadata.distribution("skytemple-files").read_text("direct_url.json") or "{}")
    commit = direct.get("vcs_info", {}).get("commit_id")
    if versions["skytemple-files"] != "1.8.5" or commit != SKYTEMPLE_COMMIT:
        raise ValueError("install requirements-offline.txt: pinned SkyTemple source not verified")
    return {"status": "pass", "sourceCommit": commit, "versions": versions,
            "implementation": "PYTHON", "defaultConfig": config.game_edition,
            "imports": [NintendoDSRom.__name__, FileType.__name__, HardcodedDungeons.__name__,
                        DungeonValidator.__name__, CharaWanHandler.__name__]}


def raw_wan_metadata(wan) -> dict:
    return {str(index): [[{"frameIndex": int(f.frameIndex), "durationTicks": int(f.duration),
                          "rawFlag": int(f.flag), "offset": list(f.offset), "shadow": list(f.shadow),
                          "markers": [key for key, present in (("rush", f.IsRushPoint()),
                                      ("hit", f.IsHitPoint()), ("return", f.IsReturnPoint())) if present]}
                         for f in sequence] for sequence in group]
            for index, group in enumerate(wan.animGroupData) if group}


def checked_pack(data: bytes):
    from skytemple_files.common.types.file_types import FileType
    if len(data) < 8:
        raise ValueError("truncated BIN_PACK header")
    count = unpack_from("<I", data, 4)[0]
    if count > 65536 or 8 + count * 8 > len(data):
        raise ValueError("invalid BIN_PACK count")
    for index in range(count):
        offset, size = unpack_from("<II", data, 8 + index * 8)
        if offset < 8 + count * 8 or size > len(data) or offset > len(data) - size:
            raise ValueError(f"BIN_PACK entry outside file: {index}")
    return FileType.BIN_PACK.deserialize(data)


def checked_raw_strings(data: bytes) -> list[bytes]:
    """공식 raw STR API를 사용하기 전에 포인터·종단·범위를 검사한다."""
    from skytemple_files.data.str.model import Str
    if len(data) < 4:
        raise ValueError("truncated STR header")
    first = unpack_from("<I", data)[0]
    if first % 4 or not 4 <= first <= len(data) or first // 4 > 65536:
        raise ValueError("invalid STR pointer table")
    pointers = list(unpack_from("<" + "I" * (first // 4), data))
    # 공식 writer는 명시적 end 포인터를 쓰지만 실제 ROM에는 마지막 문자열이
    # EOF에 닿고 별도 end entry가 없는 STR도 있다. 데이터 bytes를 포인터로 읽지 않는다.
    if pointers[-1] == len(data):
        pointers.pop()
    if any(p < first or p >= len(data) for p in pointers):
        raise ValueError("STR pointer outside string payload")
    previous = first
    for start, end in zip(pointers, pointers[1:] + [len(data)]):
        if start < previous or end < start or end > len(data) or data.find(b"\x00", start, end) < 0:
            raise ValueError("invalid STR span/terminator")
        previous = start
    strings = Str.internal__get_all_raw_strings_from(data)
    if len(strings) != len(pointers):
        raise ValueError("STR parser/table count mismatch")
    return strings


def select_floors(definitions, groups, requested):
    if requested:
        pairs = list(dict.fromkeys(requested))
    else:
        ordinary = shifted = None
        for dungeon_id, definition in enumerate(definitions):
            if definition.number_floors == 0:
                continue
            try:
                floor_index(definition, 0, groups)
                floor_index(definition, int(definition.number_floors) - 1, groups)
            except ValueError:
                continue
            if ordinary is None and definition.start_after == 0:
                ordinary = (dungeon_id, 0)
            if shifted is None and definition.start_after > 0 and definition.number_floors > 1:
                shifted = dungeon_id
        if ordinary is None or shifted is None:
            raise ValueError("general/nonzero-start_after validation floors unavailable")
        pairs = list(dict.fromkeys([ordinary, (shifted, 0),
                                   (shifted, int(definitions[shifted].number_floors) - 1)]))
    for dungeon_id, local in pairs:
        if not 0 <= dungeon_id < len(definitions):
            raise ValueError(f"unsupported general dungeon id: {dungeon_id}; dojo pending")
        floor_index(definitions[dungeon_id], local, groups)
    return pairs


def export_sprite(index, record, packs, config, identity, output):
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.graphics.chara_wan.handler import CharaWanHandler
    sprite = record["spriteIndex"]
    if sprite < 0 or any(sprite >= len(p) for p in packs.values()):
        raise ValueError(f"signed spriteIndex outside one or more packs: {sprite}")
    refs, wans = [], []
    for path, pack in packs.items():
        data = pack[sprite]
        refs.append({"sourcePath": path, "packSlot": sprite, "contentHash": sha256(data),
                     "compression": "raw_wan" if "ground" in path else "pkdpx"})
        if "ground" not in path:
            data = FileType.PKDPX.deserialize(data).decompress()
        wans.append(CharaWanHandler.deserialize(data))
    originals = deepcopy(wans)  # MergeWan이 입력 frame/piece 인덱스를 변경하므로 원본 비교용 보존.
    merged = CharaWanHandler.merge_wan(*wans)
    raw_meta = raw_wan_metadata(merged)
    missing = [name for name, wan_id in REQUIRED.items() if str(wan_id) not in raw_meta]
    if missing:
        raise ValueError(f"missing required WAN groups: {missing}")
    sheets = output / "sheets" / str(index)
    sheets.mkdir(parents=True)
    sprite_def = config.animation_names.get(sprite, config.animation_names[0])
    CharaWanHandler.export_sheets(str(sheets), merged, sprite_def)
    write_json(sheets / "raw-wan.json", raw_meta)
    asset_dir = output / "assets" / str(index)
    asset_dir.mkdir(parents=True)
    source = {"sourceKind": "rom", **identity, "mdIndex": index, "spriteIndex": sprite,
              "toolCommit": SKYTEMPLE_COMMIT, "sourceRefs": refs,
              "license": "game_assets_user_local_only", "credit": "Nintendo / Pokémon / CHUNSOFT"}
    asset, report = pack_asset(sheets, asset_dir, record["assetKey"], source, raw_meta)
    if not report["requiredComplete"]:
        raise ValueError("required exported sheet names missing")
    from .wan_verify import verify_wan_asset
    verification = verify_wan_asset(merged, asset, asset_dir, originals)
    report["romPixelMatch"] = "pass"
    report["rawWanAtlas"] = verification
    write_json(sheets / "wan-validation.json", verification)
    asset["atlas"]["file"] = f"assets/{index}/atlas.png"
    return asset, report


def extract(rom_path: Path, output: Path, requested_md: list[int], requested_floors, language=None,
            all_floors=False, allow_undecoded_text=False) -> dict:
    from ndspy.rom import NintendoDSRom
    from skytemple_files.common.types.file_types import FileType
    from skytemple_files.common.util import get_ppmdu_config_for_rom, get_binary_from_rom
    from skytemple_files.hardcoded.dungeons import HardcodedDungeons
    from skytemple_files.dungeon_data.mappa_bin.validator.validator import DungeonValidator

    before = file_hash(rom_path)
    errors, support = [], []
    try:
        rom = NintendoDSRom.fromFile(str(rom_path))
        write_json(output / "identity.json", {"romHash": before, "gameCode": rom.idCode.decode("ascii", "replace"),
                   "arm9off14": int.from_bytes(rom.arm9[0xE:0x10], "little"),
                   "edition": None, "language": None, "status": "identification_pending"})
        config = get_ppmdu_config_for_rom(rom)
        if config.game_version != "EoS":
            raise ValueError(f"only Explorers of Sky supported: {config.game_version}")
        edition = config.game_edition
        chosen_language = language or config.game_editions[edition].defaultlang
        langs = [l for l in config.string_index_data.languages if l.name == chosen_language]
        if len(langs) != 1:
            raise ValueError(f"language not supported for edition: {chosen_language}")
        lang = langs[0]
        identity = {"romHash": before, "edition": edition, "gameCode": rom.idCode.decode("ascii"),
                    "language": lang.name, "stringFilename": lang.filename,
                    "arm9off14": int.from_bytes(rom.arm9[0xE:0x10], "little")}
        write_json(output / "identity.json", identity)
        def read(path):
            return rom.getFileByName(path)
        arm9 = get_binary_from_rom(rom, config.bin_sections.arm9)
        definitions = HardcodedDungeons.get_dungeon_list(arm9, config)
        restrictions = HardcodedDungeons.get_dungeon_restrictions(arm9, config)
        mappa = FileType.MAPPA_BIN.deserialize(read("BALANCE/mappa_s.bin"))
        md_raw = FileType.MD.deserialize(read("BALANCE/monster.md"))
        item_raw = FileType.ITEM_P.deserialize(read("BALANCE/item_p.bin"))
        string_data = read("MESSAGE/" + lang.filename)
        raw_strings = checked_raw_strings(string_data)
        strings = None
        try:
            strings = FileType.STR.deserialize(string_data, string_encoding=config.string_encoding).strings
            identity["textSupport"] = "decoded_configured_encoding"
        except ValueError as error:
            if not allow_undecoded_text:
                raise
            identity.update({"configuredLanguage": lang.name, "language": None, "textSupport": "undecoded",
                             "textEncoding": config.string_encoding})
            errors.append({"stage": "strings_decode", "sourcePath": "MESSAGE/" + lang.filename,
                           "status": "unresolved", "reason": str(error),
                           "resume": "verify patched-ROM character mapping; do not assume English/CP949"})
        write_json(output / "identity.json", identity)
        md = [{"mdIndex": i, "entid": int(e.entid), "nationalDex": int(e.national_pokedex_number),
               "baseForm": int(e.base_form_index), "gender": scalar(e.gender), "spriteIndex": int(e.sprite_index),
               "assetKey": f"rom:{edition}:md:{i}"} for i, e in enumerate(md_raw.entries)]
        items = [{"recordIndex": i, "itemId": int(e.item_id), "categoryId": int(e.category),
                  "sprite": int(e.sprite), "palette": int(e.palette), "isValid": bool(e.is_valid),
                  "iconKey": None} for i, e in enumerate(item_raw.item_list)]
        members = {int(k): list(v.items) for k, v in config.dungeon_data.item_categories.items()}
        dungeon_records = []
        name_block = config.string_index_data.string_blocks["Dungeon Names (Main)"]
        for i, d in enumerate(definitions):
            record = {"dungeonId": i, "numberFloors": int(d.number_floors), "mappaIndex": int(d.mappa_index),
                      "startAfter": int(d.start_after), "numberFloorsInGroup": int(d.number_floors_in_group),
                      "playable": bool(d.number_floors), "status": "pending", "name": None,
                      "stringIndex": name_block.begin + i, "language": identity["language"],
                      "nameStatus": "decoded" if strings is not None else "undecoded",
                      "restrictions": None}
            if record["stringIndex"] >= min(name_block.end, len(raw_strings)):
                raise ValueError(f"dungeon string index outside block: {i}")
            record["name"] = strings[record["stringIndex"]] if strings is not None else None
            record["rawNameHex"] = raw_strings[record["stringIndex"]].hex()
            if i < len(restrictions):
                record["restrictions"] = {k: scalar(v) for k, v in vars(restrictions[i]).items()}
            if d.number_floors:
                try:
                    floor_index(d, 0, mappa.floor_lists)
                    floor_index(d, int(d.number_floors) - 1, mappa.floor_lists)
                    record["status"] = "range_valid"
                except ValueError as error:
                    record["status"] = "invalid_reference"
                    errors.append({"stage": "dungeon_range", "dungeonId": i, "reason": str(error)})
            else:
                record["status"] = "zero_floors"
            dungeon_records.append(record)
        try:
            validator = DungeonValidator(mappa)
            validator.validate(definitions)
            errors.extend({"stage": "DungeonValidator", "dungeonId": e.dungeon_id,
                           "type": type(e).__name__, "reason": str(e)} for e in validator.errors)
        except Exception as error:
            errors.append({"stage": "DungeonValidator", "reason": str(error), "type": type(error).__name__})
        pairs = select_floors(definitions, mappa.floor_lists, requested_floors)
        if all_floors:
            pairs = [(i, k) for i, d in enumerate(definitions) if dungeon_records[i]["status"] == "range_valid"
                     for k in range(int(d.number_floors))]
        floors = []
        for dungeon_id, local in pairs:
            try:
                floors.append(build_floor(edition, dungeon_id, local, definitions[dungeon_id], mappa.floor_lists, md, items, members))
            except Exception as error:
                errors.append({"stage": "floor_profile", "dungeonId": dungeon_id, "localFloor": local,
                               "reason": str(error), "resume": f"--floor {dungeon_id}:{local}"})
        if len(floors) != len(pairs):
            raise ValueError("selected floor normalization failed; see errors.json")
        write_json(output / "catalogue.json", {"schemaVersion": SCHEMA_VERSION, "identity": identity,
                   "toolCommits": {"skytemple-files": SKYTEMPLE_COMMIT}, "dungeons": dungeon_records,
                   "pokemon": md, "items": items, "itemCategoryMembers": members, "floors": floors,
                   "selection": {"allGeneralFloors": all_floors, "floorKeys": [f["key"] for f in floors]},
                   "dojo": {"status": "pending", "idRange": [180, 191], "reason": "special indexing not implemented"},
                   "fullFloorAudit": "executed_general" if all_floors else "pending"})
        packs = {path: checked_pack(read(path)) for path in
                 ("MONSTER/monster.bin", "MONSTER/m_ground.bin", "MONSTER/m_attack.bin")}
        if requested_md:
            candidates = list(dict.fromkeys(requested_md))
            if len(candidates) > 2 or any(not 0 <= i < len(md) for i in candidates):
                raise ValueError("choose one or two valid MD record indices")
        else:
            candidates = [i for i, e in enumerate(md) if i and e["spriteIndex"] >= 0]
        assets, used_sprites = [], set()
        for index in candidates:
            sprite = md[index]["spriteIndex"]
            if not requested_md and sprite in used_sprites:
                continue
            used_sprites.add(sprite)
            try:
                asset, report = export_sprite(index, md[index], packs, config, identity, output)
                support.append(report)
                assets.append(asset)
            except Exception as error:
                support.append({"assetKey": md[index]["assetKey"], "mdIndex": index, "spriteIndex": sprite,
                                "status": "conversion_error", "reason": str(error), "resume": f"--md {index}"})
                if requested_md:
                    raise
            if not requested_md and len(assets) == 2:
                break
        if not assets:
            raise ValueError("no representative MD has all required animations; see support.json")
        result = write_package(output, assets, support)
        write_json(output / "rom-verification.json", {"beforeHash": before, "afterHash": file_hash(rom_path),
                   "unchanged": file_hash(rom_path) == before, "representativeCount": len(assets),
                   "fullSpeciesAudit": "pending", "originalWallTime": "pending", "windowsIntegration": "pending"})
        return result
    finally:
        after = file_hash(rom_path)
        write_json(output / "errors.json", {"schemaVersion": SCHEMA_VERSION, "errors": errors,
                                            "romHashBefore": before, "romHashAfter": after, "romUnchanged": before == after})
        if support:
            write_json(output / "support.json", {"schemaVersion": SCHEMA_VERSION, "assets": support,
                       "scope": "selected_subset", "fullSpeciesAudit": "pending"})
        if after != before:
            raise ValueError("ROM changed during extraction (including error path)")
