from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import traceback

from .common import failure_report, read_json, staging, write_json


def floor_arg(value):
    try:
        dungeon, local = map(int, value.split(":"))
    except ValueError:
        raise argparse.ArgumentTypeError("floor must be dungeonId:localFloor (0-based)")
    if dungeon < 0 or local < 0:
        raise argparse.ArgumentTypeError("floor indices must be nonnegative")
    return dungeon, local


def parser():
    cli = argparse.ArgumentParser(description="MysteryDungeonPet offline assets")
    commands = cli.add_subparsers(dest="command", required=True)
    smoke = commands.add_parser("smoke")
    smoke.add_argument("--report", type=Path)
    fixture = commands.add_parser("fixture")
    fixture.add_argument("--out", type=Path, required=True)
    pack = commands.add_parser("pack")
    pack.add_argument("--sheets", type=Path, required=True)
    pack.add_argument("--provenance", type=Path, required=True)
    pack.add_argument("--asset-key", required=True)
    pack.add_argument("--out", type=Path, required=True)
    validate = commands.add_parser("validate")
    validate.add_argument("package", type=Path)
    extract = commands.add_parser("extract")
    extract.add_argument("--rom", type=Path, required=True)
    extract.add_argument("--out", type=Path, required=True)
    extract.add_argument("--md", type=int, action="append", default=[])
    extract.add_argument("--floor", type=floor_arg, action="append", default=[])
    extract.add_argument("--language")
    extract.add_argument("--all-general-floors", action="store_true")
    return cli


def main(argv=None) -> int:
    args = parser().parse_args(argv)
    reports = {}
    try:
        from .extractor import smoke
        from .packer import pack_asset, validate_package, write_package
        if args.command == "smoke":
            result = smoke()
            if args.report:
                write_json(args.report, result)
        elif args.command == "validate":
            result = validate_package(args.package)
        else:
            smoke()
            with staging(args.out) as temp:
                try:
                    if args.command == "extract":
                        from .extractor import extract
                        result = extract(args.rom, temp, args.md, args.floor, args.language, args.all_general_floors)
                    elif args.command == "fixture":
                        from .fixture import create_sheets, provenance
                        create_sheets(temp / "sheets")
                        asset, support = pack_asset(temp / "sheets", temp, "test:fixture:shapes", provenance(),
                                                    read_json(temp / "sheets/raw-wan.json"))
                        result = write_package(temp, [asset], [support])
                        write_json(temp / "errors.json", {"errors": [], "romExtraction": "pending"})
                    else:
                        raw_path = args.sheets / "raw-wan.json"
                        source = read_json(args.provenance)
                        if source.get("sourceKind") == "rom" and not raw_path.exists():
                            raise ValueError("ROM pack requires original raw-wan.json")
                        asset, support = pack_asset(args.sheets, temp, args.asset_key, source,
                                                    read_json(raw_path) if raw_path.exists() else None)
                        result = write_package(temp, [asset], [support])
                        write_json(temp / "errors.json", {"errors": []})
                except Exception:
                    for name in ("identity", "errors", "support"):
                        path = temp / f"{name}.json"
                        if path.exists():
                            reports[name] = read_json(path)
                    raise
        print(json.dumps(result, ensure_ascii=False, sort_keys=True))
        return 0
    except Exception as error:
        point = traceback.extract_tb(error.__traceback__)[-1]
        if getattr(args, "out", None):
            report = failure_report(args.out, args.command, error, reports=reports,
                                    function=point.name, module=Path(point.filename).name,
                                    resumeCommand=f"python -m tools.mdpet {args.command} (same inputs, new --out)")
            print(f"{type(error).__name__}: {error}; report: {report}", file=sys.stderr)
        else:
            print(f"{type(error).__name__}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
