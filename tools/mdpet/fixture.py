from __future__ import annotations

from pathlib import Path
import xml.etree.ElementTree as ET

from PIL import Image

from . import OPTIONAL, REQUIRED
from .common import write_json


def create_sheets(root: Path) -> None:
    """테스트용 도형. 원작 그래픽·종/폼/ROM ID를 만들지 않는다."""
    root.mkdir(parents=True, exist_ok=True)
    xml = ET.Element("AnimData")
    ET.SubElement(xml, "ShadowSize").text = "1"
    anims = ET.SubElement(xml, "Anims")
    raw_wan = {}
    for name, wan_id in {**REQUIRED, **OPTIONAL}.items():
        node = ET.SubElement(anims, "Anim")
        ET.SubElement(node, "Name").text = name
        ET.SubElement(node, "Index").text = str(wan_id)
        if name in ("Idle", "EventSleep", "Laying"):
            ET.SubElement(node, "CopyOf").text = "Walk" if name == "Idle" else "Idle"
            continue
        width, height = 16, 24
        ticks = [30, 35] if name == "Sleep" else [6, 8, 6, 8] if name == "Eat" else [8, 10, 8]
        rows = 1 if name in ("Sleep", "Eat") else 8
        ET.SubElement(node, "FrameWidth").text = str(width)
        ET.SubElement(node, "FrameHeight").text = str(height)
        ET.SubElement(node, "HitFrame").text = "1"
        durations = ET.SubElement(node, "Durations")
        for tick in ticks:
            ET.SubElement(durations, "Duration").text = str(tick)
        images = [Image.new("RGBA", (width * len(ticks), height * rows)) for _ in range(3)]
        directions = []
        for row in range(rows):
            frames = []
            for col, tick in enumerate(ticks):
                x, y = col * width, row * height
                for px in range(5, 11):
                    for py in range(7, 16):
                        images[0].putpixel((x + px, y + py), (20 + 20 * row, 40 + 35 * col, 90, 255))
                images[0].putpixel((x + 4, y + 8), (0, 0, 0, 255))  # 불투명 검정 보존
                images[0].putpixel((x + 11, y + 8), (120, 60, 20, 128))  # straight alpha 보존
                images[1].putpixel((x + 8, y + 8), (0, 0, 0, 255))
                images[1].putpixel((x + 8, y + 12), (255, 0, 255, 255))  # 손 marker 겹침
                images[1].putpixel((x + 8, y + 14), (0, 255, 0, 255))
                images[2].putpixel((x + 8, y + 16), (255, 255, 255, 255))
                images[2].putpixel((x + 7, y + 16), (0, 255, 0, 255))
                frames.append({"frameIndex": col, "durationTicks": tick + row,
                               "rawFlag": 2 if col == 1 else 0, "offset": [0, 0], "shadow": [0, 0],
                               "markers": ["hit"] if col == 1 else []})
            # WAN export는 짧은 방향을 padding할 수 있음. 본래 프레임 수로 복원.
            if rows == 8 and row == 7:
                frames = frames[:-1]
            directions.append(frames)
        raw_wan[str(wan_id)] = directions
        for suffix, image in zip(("Anim", "Offsets", "Shadow"), images):
            image.save(root / f"{name}-{suffix}.png")
    for wan_id in (7, 13, 27):
        raw_wan[str(wan_id)] = raw_wan["0"]
    ET.ElementTree(xml).write(root / "AnimData.xml", encoding="utf-8", xml_declaration=True)
    write_json(root / "raw-wan.json", raw_wan)


def provenance() -> dict:
    return {"sourceKind": "test", "romHash": None, "edition": None, "mdIndex": None, "spriteIndex": None,
            "sourcePath": "tools/mdpet/fixture.py", "license": "project_test_fixture",
            "credit": "MysteryDungeonPet generated test shapes", "status": "test_only"}
