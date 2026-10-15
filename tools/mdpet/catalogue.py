from __future__ import annotations

from enum import Enum
from functools import reduce
from math import gcd


def scalar(value):
    return scalar(value.value) if isinstance(value, Enum) else value


def cdf_weights(values: list[int]) -> list[int]:
    """0은 누락 행. 마지막 비영 누적값과 차분 후 GCD 축약."""
    previous = 0
    differences = []
    for value in values:
        if type(value) is not int or not 0 <= value <= 65534:
            raise ValueError(f"invalid CDF: {value}")
        if value == 0:
            differences.append(0)
            continue
        if value < previous:
            raise ValueError(f"non-monotonic CDF: {value} < {previous}")
        differences.append(value - previous)
        previous = value
    divisor = reduce(gcd, differences, 0)
    return [v // divisor for v in differences] if divisor else differences


def weighted(entries: list[tuple[int, int]], context: str) -> dict:
    weights = cdf_weights([raw for _, raw in entries])
    return {"context": context, "status": "ok" if any(weights) else "empty",
            "entries": [{"originalId": key, "rawCDF": raw, "relativeWeight": weight}
                        for (key, raw), weight in zip(entries, weights)], "guaranteed": []}


def floor_index(definition, local_floor: int, groups) -> int:
    if not 0 <= local_floor < definition.number_floors:
        raise ValueError("localFloor outside dungeon")
    group = int(definition.mappa_index)
    index = int(definition.start_after) + local_floor
    if not 0 <= group < len(groups) or not 0 <= index < len(groups[group]):
        raise ValueError(f"group/floor outside MAPPA: {group}/{index}")
    return index


def item_distribution(item_list, item_records: list[dict], context: str, category_members=None) -> dict:
    """MAPPA item key는 item_p 레코드 인덱스. item_id는 별도 필드."""
    categories = sorted((int(k), int(v)) for k, v in item_list.categories.items())
    # guaranteed 카테고리도 랜덤 가중치와 분리한다.
    category_random = [(k, v) for k, v in categories if v != 65535]
    result = {"context": context, "categories": weighted(category_random, context + ":category"),
              "conditionalItems": [], "guaranteed": [],
              "rawCategories": [{"categoryId": k, "rawCDF": v} for k, v in categories]}
    result["categories"]["guaranteed"] = [k for k, v in categories if v == 65535]
    grouped = {}
    for index, raw in sorted(item_list.items.items()):
        index, raw = int(index), int(raw)
        if not 0 <= index < len(item_records):
            raise ValueError(f"missing item_p record: {index}")
        if category_members is None:
            category = item_records[index]["categoryId"]
        else:
            memberships = [cat for cat, members in category_members.items() if index in members]
            if len(memberships) != 1:
                raise ValueError(f"item category reference ambiguous/missing: {index}: {memberships}")
            category = memberships[0]
        if raw == 65535:
            result["guaranteed"].append({"recordIndex": index, "itemId": item_records[index]["itemId"],
                                         "categoryId": category, "rawCDF": raw})
        else:
            grouped.setdefault(category, []).append((index, raw))
    for category, rows in sorted(grouped.items()):
        result["conditionalItems"].append({"categoryId": category,
                                          **weighted(rows, context + f":category:{category}")})
    return result


def monster_distributions(monsters, md_count: int) -> dict:
    normal, mh, special = [], [], []
    normal_weights = cdf_weights([int(r.main_spawn_weight) for r in monsters])
    mh_weights = cdf_weights([int(r.monster_house_spawn_weight) for r in monsters])
    for i, row in enumerate(monsters):
        key = int(row.md_index)
        if not 0 <= key < md_count:
            raise ValueError(f"missing MD reference: {key}")
        raw = {"mdIndex": key, "level": int(row.level), "normalCDF": int(row.main_spawn_weight),
               "monsterHouseCDF": int(row.monster_house_spawn_weight)}
        raw["normalRelativeWeight"] = normal_weights[i]
        raw["monsterHouseRelativeWeight"] = mh_weights[i]
        # 종료/더미/케클레온 관련 행은 원본 표에는 보존하되 일반 후보로 사용하지 않음.
        if key in (0, 0x229, 383, 983):
            special.append(raw)
        else:
            normal.append({"originalId": key, "rawCDF": raw["normalCDF"], "relativeWeight": normal_weights[i]})
            mh.append({"originalId": key, "rawCDF": raw["monsterHouseCDF"], "relativeWeight": mh_weights[i]})
    def distribution(rows, context):
        return {"context": context, "status": "ok" if any(r["relativeWeight"] for r in rows) else "empty",
                "entries": rows, "guaranteed": []}
    return {"normal": distribution(normal, "normal"), "monsterHouse": distribution(mh, "monster_house"),
            "specialRows": special,
            "rawRows": [{"mdIndex": int(r.md_index), "level": int(r.level),
                         "normalCDF": int(r.main_spawn_weight),
                         "monsterHouseCDF": int(r.monster_house_spawn_weight)} for r in monsters]}


def layout_record(layout) -> dict:
    # protocol 정의로만 읽는다. 구현체의 임의 내부 캐시는 직렬화하지 않는다.
    from skytemple_files.dungeon_data.mappa_bin.protocol import MappaFloorLayoutProtocol
    from skytemple_files.dungeon_data.mappa_bin.protocol import MappaFloorTerrainSettingsProtocol
    result = {name: scalar(getattr(layout, name)) for name in MappaFloorLayoutProtocol.__annotations__
              if name != "terrain_settings"}
    result["terrain_settings"] = {name: bool(getattr(layout.terrain_settings, name))
                                  for name in MappaFloorTerrainSettingsProtocol.__annotations__}
    return result


def build_floor(edition, dungeon_id, local_floor, definition, groups, md, items, category_members=None) -> dict:
    index = floor_index(definition, local_floor, groups)
    floor = groups[int(definition.mappa_index)][index]
    layout = layout_record(floor.layout)
    contexts = ("floor_items", "shop_items", "monster_house_items", "buried_items", "unk_items1", "unk_items2")
    return {"key": {"edition": edition, "dungeonId": dungeon_id, "localFloor": local_floor},
            "groupIndex": int(definition.mappa_index), "groupFloor": index,
            "tilesetId": int(floor.layout.tileset_id), "rawLayout": layout,
            "monsters": monster_distributions(floor.monsters, len(md)),
            "itemContexts": [item_distribution(getattr(floor, ctx), items, ctx, category_members) for ctx in contexts],
            "traps": {str(int(k)): int(v) for k, v in floor.traps.weights.items()}}
