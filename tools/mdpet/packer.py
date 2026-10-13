from __future__ import annotations

from pathlib import Path
import xml.etree.ElementTree as ET

from PIL import Image

from . import ASSET_VERSION, OPTIONAL, REQUIRED, SCHEMA_VERSION
from .common import file_hash, integer, read_json, safe_file, sha256, transform_identity, write_json

MAX_PIXELS = 16 * 1024 * 1024
MAX_FRAMES = 16384


def load_png(path: Path) -> Image.Image:
    if path.stat().st_size > 32 * 1024 * 1024:
        raise ValueError("PNG byte limit exceeded")
    with Image.open(path) as source:
        if source.format != "PNG" or source.width * source.height > MAX_PIXELS:
            raise ValueError("invalid PNG format/pixel count")
        source.load()
        return source.convert("RGBA")


def nodes_from_xml(path: Path) -> tuple[int, dict]:
    if path.stat().st_size > 1024 * 1024:
        raise ValueError("XML byte limit exceeded")
    data = path.read_bytes()
    if b"<!DOCTYPE" in data or b"<!ENTITY" in data:
        raise ValueError("XML entities are not supported")
    root = ET.fromstring(data)
    if root.tag != "AnimData":
        raise ValueError("expected AnimData XML")
    shadow = integer(int(root.findtext("ShadowSize", "-1")), "ShadowSize", 0, 2)
    nodes = {}
    for node in root.findall("Anims/Anim"):
        name = node.findtext("Name")
        if not name or not name.replace("_", "").isalnum() or name in nodes:
            raise ValueError("duplicate/unsafe animation name")
        nodes[name] = node
    if not nodes or len(nodes) > 256:
        raise ValueError("invalid animation count")
    return shadow, nodes


def resolve_alias(nodes, name: str) -> str:
    visited = set()
    while True:
        if name in visited:
            raise ValueError(f"alias cycle: {name}")
        if name not in nodes:
            raise ValueError(f"missing alias target: {name}")
        visited.add(name)
        target = nodes[name].findtext("CopyOf")
        if target is None:
            return name
        name = target


def point_markers(image: Image.Image, shadow=False) -> dict:
    from skytemple_files.graphics.chara_wan.utils import getOffsetFromRGB
    values = getOffsetFromRGB(image, (0, 0, image.width, image.height),
                              not shadow, not shadow, not shadow, not shadow, shadow)
    if shadow:
        return {"shadow": list(values[4]) if values[4] is not None else None}
    return {key: list(values[i]) if values[i] is not None else None
            for i, key in enumerate(("head", "lhand", "center", "rhand"))}


def shifted(point, origin):
    return [point[0] - origin[0], point[1] - origin[1]] if point is not None else None


class Atlas:
    def __init__(self):
        self.cells = []
        self.by_hash = {}
        self.pixels = 0

    def add(self, image: Image.Image) -> list[int]:
        key = (image.size, sha256(image.tobytes()))
        if key not in self.by_hash:
            self.pixels += image.width * image.height
            if self.pixels > MAX_PIXELS:
                raise ValueError("atlas pixel budget exceeded")
            self.by_hash[key] = len(self.cells)
            self.cells.append(image)
        return [self.by_hash[key], 0, image.width, image.height]

    def finish(self, path: Path, frames: list[dict]):
        if not self.cells:
            raise ValueError("no atlas frames")
        width = max(256, max(i.width for i in self.cells))
        cursor_x = cursor_y = row_h = 0
        positions = []
        for cell in self.cells:
            if cursor_x + cell.width > width:
                cursor_y += row_h
                cursor_x = row_h = 0
            positions.append((cursor_x, cursor_y))
            cursor_x += cell.width
            row_h = max(row_h, cell.height)
        height = cursor_y + row_h
        if width > 4096 or height > 4096 or width * height > MAX_PIXELS:
            raise ValueError("atlas dimensions exceeded")
        atlas = Image.new("RGBA", (width, height))
        for cell, position in zip(self.cells, positions):
            atlas.paste(cell, position)
        for frame in frames:
            for key in ("rect", "shadowRect"):
                rect = frame[key]
                x, y = positions[rect[0]]
                frame[key] = [x, y, rect[2], rect[3]]
        atlas.save(path, optimize=False, compress_level=9)
        return {"file": path.name, "width": width, "height": height,
                "sha256": file_hash(path), "decodedBytes": width * height * 4,
                "byteSize": path.stat().st_size, "format": "straight_rgba8"}


def pack_asset(sheets: Path, output: Path, asset_key: str, provenance: dict, raw_wan=None, extra_animations=None) -> tuple[dict, dict]:
    provenance = {**provenance, "transform": transform_identity()}
    shadow_size, nodes = nodes_from_xml(sheets / "AnimData.xml")
    # 전체 alias graph는 선택 범위 바깥에도 검사한다.
    for name in nodes:
        resolve_alias(nodes, name)
    requested = {**REQUIRED, **OPTIONAL, **(extra_animations or {})}
    selected = [name for name in requested if name in nodes]
    for name in list(selected):
        target = nodes[name].findtext("CopyOf")
        while target:
            if target not in selected:
                selected.append(target)
            target = nodes[target].findtext("CopyOf")
    selected = sorted(selected)
    atlas = Atlas()
    clips, all_frames = [], []
    report = {"assetKey": asset_key, "sourceKind": provenance["sourceKind"], "animations": [],
              "romPixelMatch": "pending", "originalWallTime": "pending"}
    for name, expected_id in REQUIRED.items():
        if name not in nodes:
            report["animations"].append({"name": name, "wanId": expected_id, "status": "missing"})
    for name in selected:
        node = nodes[name]
        wan_id = int(node.findtext("Index")) if node.find("Index") is not None else None
        if name in requested and wan_id != requested[name]:
            raise ValueError(f"unexpected WAN id: {name}: {wan_id}")
        target = node.findtext("CopyOf")
        clip = {"name": name, "wanId": wan_id, "aliasOf": target, "directions": [],
                "sourceTiming": {"tickHz": None, "provisionalTickHz": 60,
                                 "rawSpeed": None, "rawLoop": None, "scriptFlags": None}}
        if target is not None:
            if raw_wan is not None:
                if str(wan_id) not in raw_wan:
                    raise ValueError(f"missing alias WAN metadata: {wan_id}")
                clip["rawDirections"] = raw_wan[str(wan_id)]
            clips.append(clip)
            report["animations"].append({"name": name, "wanId": wan_id, "status": "alias_exact", "aliasOf": target})
            continue
        width = integer(int(node.findtext("FrameWidth", "0")), "FrameWidth", 1, 1024)
        height = integer(int(node.findtext("FrameHeight", "0")), "FrameHeight", 1, 1024)
        durations = [integer(int(n.text), "durationTicks", 1, 65535) for n in node.findall("Durations/Duration")]
        if not durations or len(durations) > 1024:
            raise ValueError("invalid duration count")
        images = [load_png(safe_file(sheets, f"{name}-{suffix}.png")) for suffix in ("Anim", "Offsets", "Shadow")]
        anim, offsets, shadows = images
        if len({i.size for i in images}) != 1 or anim.width != width * len(durations) or anim.height % height:
            raise ValueError(f"sheet dimensions/durations disagree: {name}")
        rows = anim.height // height
        if rows not in (1, 8):
            raise ValueError(f"unsupported direction rows: {rows}")
        xml_markers = {}
        for tag, key in (("RushFrame", "rush"), ("HitFrame", "hit"), ("ReturnFrame", "return")):
            if node.find(tag) is not None:
                xml_markers[key] = integer(int(node.findtext(tag)), tag, 0, len(durations) - 1)
        raw_directions = raw_wan.get(str(wan_id)) if raw_wan else None
        if raw_wan is not None and raw_directions is None:
            raise ValueError(f"missing original WAN metadata: {wan_id}")
        if raw_directions is not None and len(raw_directions) != rows:
            raise ValueError("WAN rows disagree with exported sheet")
        clip.update({"rows": rows, "directional": rows == 8, "frameSize": [width, height],
                     "xmlDurationTicks": durations, "xmlMarkers": xml_markers})
        for row in range(rows):
            frames = []
            count = len(raw_directions[row]) if raw_directions is not None else len(durations)
            if not 1 <= count <= len(durations):
                raise ValueError("WAN frame count disagrees with sheet")
            for col in range(count):
                box = (col * width, row * height, (col + 1) * width, (row + 1) * height)
                cell, marker_cell, shadow_cell = [i.crop(box) for i in images]
                points = point_markers(marker_cell)
                points.update(point_markers(shadow_cell, True))
                # center/white shadow 누락은 자동 대체하지 않는다.
                if points["center"] is None or points["shadow"] is None:
                    raise ValueError(f"missing center/shadow marker: {name}/{row}/{col}")
                bounds = [i.getbbox() for i in (cell, shadow_cell) if i.getbbox()]
                crop = (min(b[0] for b in bounds), min(b[1] for b in bounds),
                        max(b[2] for b in bounds), max(b[3] for b in bounds)) if bounds else (0, 0, 1, 1)
                origin = crop[:2]
                body_crop, shadow_crop = cell.crop(crop), shadow_cell.crop(crop)
                for original, cropped in ((cell, body_crop), (shadow_cell, shadow_crop)):
                    restored = Image.new("RGBA", original.size)
                    restored.paste(cropped, origin)
                    if restored.tobytes() != original.tobytes():
                        raise ValueError("crop/reassembly RGBA mismatch")
                raw = raw_directions[row][col] if raw_directions is not None else None
                ticks = integer(raw["durationTicks"] if raw else durations[col], "durationTicks", 1, 65535)
                markers = raw["markers"] if raw else [key for key, index in xml_markers.items() if index == col]
                frame = {"rect": atlas.add(body_crop), "shadowRect": atlas.add(shadow_crop),
                         "cropOrigin": list(origin), "anchor": shifted([width // 2, height // 2 + 4], origin),
                         "points": {key: shifted(point, origin) for key, point in points.items()},
                         "durationTicks": ticks, "markers": markers, "rawWan": raw,
                         "sourceRGBAHash": sha256(cell.tobytes()), "sourceShadowHash": sha256(shadow_cell.tobytes())}
                frames.append(frame)
                all_frames.append(frame)
                if len(all_frames) > MAX_FRAMES:
                    raise ValueError("frame count exceeded")
            clip["directions"].append(frames)
        clips.append(clip)
        status = {"rom": "exact_rom", "public": "exact_public", "test": "test_fixture"}[provenance["sourceKind"]]
        report["animations"].append({"name": name, "wanId": wan_id, "status": status, "rows": rows,
                                     "framesByDirection": [len(r) for r in clip["directions"]],
                                     "markerValidity": "pass", "sheetAtlasRGBA": "pass"})
    atlas_info = atlas.finish(output / "atlas.png", all_frames)
    asset = {"assetKey": asset_key, "source": provenance, "atlas": atlas_info,
             "shadowSize": shadow_size, "shadowEncoding": "skytemple_rgb_size_mask_with_white_origin",
             "animations": clips}
    report["requiredComplete"] = all(name in nodes for name in REQUIRED)
    return asset, report


def validate_package(root: Path) -> dict:
    manifest = read_json(root / "manifest.json")
    if manifest.get("schemaVersion") != SCHEMA_VERSION or manifest.get("assetVersion") != ASSET_VERSION:
        raise ValueError("unsupported schema/asset version")
    assets = manifest.get("assets")
    if not isinstance(assets, list) or not 1 <= len(assets) <= 32:
        raise ValueError("invalid asset count")
    keys = set()
    frame_count = 0
    for asset in assets:
        if not isinstance(asset["assetKey"], str) or asset["assetKey"] in keys:
            raise ValueError("duplicate/invalid asset key")
        keys.add(asset["assetKey"])
        source = asset["source"]
        if source["sourceKind"] not in ("rom", "public", "test"):
            raise ValueError("invalid provenance")
        if source["sourceKind"] == "test" and any(source.get(k) is not None for k in ("romHash", "edition", "mdIndex", "spriteIndex")):
            raise ValueError("test fixture contains fabricated ROM identity")
        info = asset["atlas"]
        atlas_path = safe_file(root, info["file"])
        if file_hash(atlas_path) != info["sha256"] or atlas_path.stat().st_size != info["byteSize"]:
            raise ValueError("atlas hash/byte size mismatch")
        atlas = load_png(atlas_path)
        if atlas.size != (info["width"], info["height"]) or atlas.width * atlas.height * 4 != info["decodedBytes"]:
            raise ValueError("atlas dimensions/decoded size mismatch")
        integer(asset["shadowSize"], "shadowSize", 0, 2)
        clips = asset["animations"]
        if not 1 <= len(clips) <= 256:
            raise ValueError("invalid clip count")
        by_name = {clip["name"]: clip for clip in clips}
        if len(by_name) != len(clips):
            raise ValueError("duplicate clip")
        for clip in clips:
            current, visited = clip, set()
            while current["aliasOf"] is not None:
                name = current["name"]
                if name in visited or current["aliasOf"] not in by_name:
                    raise ValueError("alias cycle/missing target")
                visited.add(name)
                current = by_name[current["aliasOf"]]
            if clip["aliasOf"] is not None:
                if clip["directions"]:
                    raise ValueError("alias owns redundant frames")
                continue
            integer(clip["rows"], "rows", 1, 8)
            if clip["rows"] not in (1, 8) or len(clip["directions"]) != clip["rows"] or clip["directional"] != (clip["rows"] == 8):
                raise ValueError("direction row mismatch")
            width, height = clip["frameSize"]
            integer(width, "cell width", 1, 1024)
            integer(height, "cell height", 1, 1024)
            for direction in clip["directions"]:
                if not 1 <= len(direction) <= 1024:
                    raise ValueError("invalid direction frame count")
                for frame in direction:
                    frame_count += 1
                    if frame_count > MAX_FRAMES:
                        raise ValueError("frame count exceeded")
                    integer(frame["durationTicks"], "durationTicks", 1, 65535)
                    for key in ("rect", "shadowRect"):
                        x, y, w, h = frame[key]
                        integer(x, "rect x", 0, atlas.width)
                        integer(y, "rect y", 0, atlas.height)
                        integer(w, "rect w", 1, atlas.width)
                        integer(h, "rect h", 1, atlas.height)
                        if x + w > atlas.width or y + h > atlas.height:
                            raise ValueError("atlas rect out of bounds")
                    cx, cy = frame["cropOrigin"]
                    integer(cx, "crop x", 0, width - 1)
                    integer(cy, "crop y", 0, height - 1)
                    w, h = frame["rect"][2:]
                    if frame["shadowRect"][2:] != [w, h] or cx + w > width or cy + h > height:
                        raise ValueError("crop outside original cell")
                    if frame["anchor"] != [width // 2 - cx, height // 2 + 4 - cy]:
                        raise ValueError("anchor/crop origin mismatch")
                    for point in frame["points"].values():
                        if point is not None:
                            px, py = point
                            integer(px, "marker x", -1024, 2048)
                            integer(py, "marker y", -1024, 2048)
                    if any(m not in ("rush", "hit", "return") for m in frame["markers"]):
                        raise ValueError("unknown marker")
                    for rect_key, hash_key in (("rect", "sourceRGBAHash"), ("shadowRect", "sourceShadowHash")):
                        x, y, w, h = frame[rect_key]
                        restored = Image.new("RGBA", (width, height))
                        restored.paste(atlas.crop((x, y, x + w, y + h)), (cx, cy))
                        if sha256(restored.tobytes()) != frame[hash_key]:
                            raise ValueError("packed frame RGBA reassembly mismatch")
    return {"status": "pass", "assetCount": len(assets), "frameCount": frame_count,
            "manifestHash": file_hash(root / "manifest.json"),
            "romExtraction": "pending" if all(a["source"]["sourceKind"] != "rom" for a in assets) else "requires_original_check"}


def write_package(root: Path, assets: list[dict], support: list[dict]) -> dict:
    write_json(root / "manifest.json", {"schemaVersion": SCHEMA_VERSION, "assetVersion": ASSET_VERSION,
                                        "assets": assets})
    write_json(root / "support.json", {"schemaVersion": SCHEMA_VERSION, "assets": support,
                                       "scope": "selected_subset", "fullSpeciesAudit": "pending"})
    result = validate_package(root)
    write_json(root / "validation.json", result)
    return result
