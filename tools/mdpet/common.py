from __future__ import annotations

import hashlib
import json
import os
import shutil
import tempfile
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical(data) -> bytes:
    return (json.dumps(data, ensure_ascii=False, sort_keys=True, indent=2, allow_nan=False) + "\n").encode("utf-8")


def write_json(path: Path, data) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical(data))


def read_json(path: Path):
    if path.stat().st_size > 32 * 1024 * 1024:
        raise ValueError(f"JSON byte limit exceeded: {path.name}")
    return json.loads(path.read_text(encoding="utf-8"))


def transform_identity() -> dict:
    root = Path(__file__).parent
    hashes = {p.name: file_hash(p) for p in sorted(root.glob("*.py"))}
    return {"version": 1, "sourceHash": sha256(canonical(hashes)),
            "dependencyVersions": {name: __import__("importlib.metadata", fromlist=["version"]).version(name)
                                   for name in ("skytemple-files", "skytemple-rust", "Pillow", "ndspy")}}


def integer(value, label: str, minimum=0, maximum=0xFFFFFFFF) -> int:
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{label}: integer outside [{minimum}, {maximum}]")
    return value


def safe_file(root: Path, name: str) -> Path:
    if not isinstance(name, str) or not name or "\\" in name:
        raise ValueError("invalid package file name")
    rel = Path(name)
    if rel.is_absolute() or ".." in rel.parts:
        raise ValueError("package path escapes root")
    path = root / rel
    if not path.resolve().is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError(f"missing/unsafe package file: {name}")
    return path


@contextmanager
def staging(output: Path):
    """새 불변 출력에만 확정한다. 정상 패키지를 덮어쓰지 않는다."""
    output = output.absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise FileExistsError(f"output exists; choose a new directory: {output}")
    temp = Path(tempfile.mkdtemp(prefix=".mdpet-", dir=output.parent))
    try:
        yield temp
        if output.exists():
            raise FileExistsError(f"output appeared during conversion: {output}")
        os.rename(temp, output)
    finally:
        if temp.exists():
            shutil.rmtree(temp)


def failure_report(output: Path, stage: str, error: Exception, **context) -> Path:
    # 실행시각·로컬 경로는 의미 데이터와 분리. 실패해도 정상 출력은 유지.
    path = output.absolute().with_name(output.name + ".failure.json")
    write_json(path, {"status": "failed", "stage": stage, "errorType": type(error).__name__,
                      "reason": str(error), "resume": stage, "context": context,
                      "recordedKST": datetime.now(ZoneInfo("Asia/Seoul")).isoformat()})
    return path
