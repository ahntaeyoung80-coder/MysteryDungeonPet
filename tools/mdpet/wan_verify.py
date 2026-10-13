from __future__ import annotations

from pathlib import Path

from PIL import Image

from .packer import load_png


def render_wan_frame(wan, sequence_frame, size) -> Image.Image:
    """시트 export를 호출하지 않고 기존 WAN piece API로 원본 프레임을 그린다."""
    from skytemple_files.graphics.chara_wan.model import MINUS_FRAME
    pieces = wan.frameData[sequence_frame.frameIndex]
    canvas = Image.new("RGBA", tuple(size))
    anchor = (size[0] // 2, size[1] // 2 + 4)
    draw_origin = (-anchor[0] - sequence_frame.offset[0], -anchor[1] - sequence_frame.offset[1])
    for position in range(len(pieces) - 1, -1, -1):
        piece = pieces[position]
        parent = piece.imgIndex
        if parent == MINUS_FRAME:
            prior = next((p for p in reversed(pieces[:position])
                          if p.imgIndex != MINUS_FRAME and p.getTileNum() == piece.getTileNum()), None)
            if prior is None:
                raise ValueError("WAN piece has no matching parent")
            parent = prior.imgIndex
        image = piece.GeneratePiece(wan.imgData, wan.customPalette, parent)
        piece.DrawOn(canvas, image, draw_origin)
    return canvas


def verify_wan_asset(wan, asset: dict, root: Path, original_wans=()) -> dict:
    atlas = load_png(root / asset["atlas"]["file"])
    clips = {c["name"]: c for c in asset["animations"]}
    compared = 0
    source_compared = 0
    for clip in clips.values():
        resolved = clip
        while resolved["aliasOf"] is not None:
            resolved = clips[resolved["aliasOf"]]
        if clip["wanId"] is None:
            continue  # XML supplementary name; primary WAN-ID clip is checked.
        group = wan.animGroupData[clip["wanId"]]
        if len(group) != resolved["rows"]:
            raise ValueError(f"original WAN direction mismatch: {clip['name']}")
        for row, sequence in enumerate(group):
            frames = resolved["directions"][row]
            if len(sequence) != len(frames):
                raise ValueError(f"original WAN frame count mismatch: {clip['name']}/{row}")
            for col, (original, packed) in enumerate(zip(sequence, frames)):
                size = resolved["frameSize"]
                x, y, w, h = packed["rect"]
                cx, cy = packed["cropOrigin"]
                restored = Image.new("RGBA", tuple(size))
                restored.paste(atlas.crop((x, y, x + w, y + h)), (cx, cy))
                direct = render_wan_frame(wan, original, size)
                if restored.tobytes() != direct.tobytes():
                    raise ValueError(f"original WAN/atlas RGBA mismatch: {clip['name']}/{row}/{col}")
                for source_wan in original_wans:
                    if clip["wanId"] >= len(source_wan.animGroupData) or not source_wan.animGroupData[clip["wanId"]]:
                        continue
                    source_group = source_wan.animGroupData[clip["wanId"]]
                    if len(source_group) != len(group) or len(source_group[row]) != len(sequence):
                        raise ValueError(f"merge changed original direction/frame count: {clip['name']}")
                    source_frame = source_group[row][col]
                    if render_wan_frame(source_wan, source_frame, size).tobytes() != direct.tobytes():
                        raise ValueError(f"merge changed original WAN pixels: {clip['name']}/{row}/{col}")
                    source_offset = source_wan.offsetData[source_frame.frameIndex]
                    merged_offset = wan.offsetData[original.frameIndex]
                    for key in ("head", "lhand", "rhand", "center"):
                        if getattr(source_offset, key) != getattr(merged_offset, key):
                            raise ValueError(f"merge changed original WAN {key}")
                    if (source_frame.duration, source_frame.flag, source_frame.offset, source_frame.shadow,
                        source_frame.IsRushPoint()) != (original.duration, original.flag, original.offset,
                                                        original.shadow, original.IsRushPoint()):
                        raise ValueError("merge changed original timing/flag/offset/shadow")
                    source_compared += 1
                anchor = (size[0] // 2, size[1] // 2 + 4)
                offsets = wan.offsetData[original.frameIndex]
                for key in ("head", "lhand", "rhand", "center"):
                    point = getattr(offsets, key)
                    expected = [point[0] + original.offset[0] + anchor[0] - cx,
                                point[1] + original.offset[1] + anchor[1] - cy]
                    if packed["points"][key] != expected:
                        raise ValueError(f"original WAN/atlas {key} mismatch: {clip['name']}/{row}/{col}")
                expected_shadow = [original.shadow[0] + anchor[0] - cx, original.shadow[1] + anchor[1] - cy]
                if packed["points"]["shadow"] != expected_shadow:
                    raise ValueError(f"original WAN/atlas shadow mismatch: {clip['name']}/{row}/{col}")
                raw = (clip.get("rawDirections") or resolved.get("rawDirections"))
                ticks = raw[row][col]["durationTicks"] if raw else packed["durationTicks"]
                if ticks != int(original.duration):
                    raise ValueError("original WAN/atlas tick mismatch")
                compared += 1
    return {"status": "pass", "comparedFrames": compared, "originalSourceFrames": source_compared,
            "method": "direct WAN GeneratePiece/DrawOn vs atlas cell, raw marker/shadow/tick positions",
            "scope": "original pack WANs → merged WAN → selected atlas clips; game wall time pending"}
