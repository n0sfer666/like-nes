#!/usr/bin/env python3
"""Compare two Neon Rumble dump stems, each with .replay and .snapshot files."""
import argparse
from pathlib import Path
import re
import sys

HEADER = ("dump_version", "protocol", "match_hash", "tick")
FIELD = re.compile(r"[a-z0-9_.]+=(-?[0-9]+)\Z")
MAX_DUMP = 2 * 1024 * 1024


def fields(path):
    with path.open("rb") as stream:
        data = stream.read(MAX_DUMP + 1)
    if len(data) > MAX_DUMP:
        raise ValueError(f"dump exceeds size limit: {path}")
    result = {}
    for line in data.decode("ascii").splitlines():
        if not FIELD.fullmatch(line):
            raise ValueError(f"malformed dump field: {path}")
        name, value = line.split("=", 1)
        if name in result:
            raise ValueError(f"duplicate field: {name}")
        result[name] = int(value)
    return result


def bundle(stem):
    replay = fields(Path(str(stem) + ".replay"))
    snapshot = fields(Path(str(stem) + ".snapshot"))
    if any(key not in replay or key not in snapshot or replay[key] != snapshot[key] for key in HEADER):
        raise ValueError(f"incompatible replay and snapshot: {stem}")
    if replay["dump_version"] != 1 or replay["protocol"] < 1:
        raise ValueError(f"unsupported dump version: {stem}")
    first, last = replay["begin"], replay["tick"]
    if first < 0 or last < first or last - first > 64:
        raise ValueError(f"invalid replay range: {stem}")
    rows = {}
    for key, value in replay.items():
        if key.startswith("frames."):
            _, number, field = key.split(".", 2)
            rows.setdefault(int(number), {})[field] = value
    if sorted(rows) != list(range(first, last + 1)) or any("hash" not in row for row in rows.values()):
        raise ValueError(f"incomplete replay: {stem}")
    if snapshot["state_hash"] != rows[last]["hash"] or not any(k.startswith("world.") for k in snapshot):
        raise ValueError(f"incomplete snapshot: {stem}")
    return replay, snapshot, rows


def compare(left, right):
    a, sa, ra = bundle(left)
    b, sb, rb = bundle(right)
    if any(a[k] != b[k] for k in HEADER[:-1]):
        raise ValueError("dumps come from different protocol versions or match settings")
    overlap = sorted(ra.keys() & rb.keys())
    if not overlap:
        raise ValueError("confirmed replay ranges do not overlap")
    first = next((tick for tick in overlap if ra[tick] != rb[tick]), None)
    field = None
    if a["tick"] == b["tick"]:
        names = [k for k in dict.fromkeys((*sa, *sb)) if k.startswith("world.")]
        field = next((name for name in names if sa.get(name) != sb.get(name)), None)
    if first is None and field is None:
        print(f"equal over confirmed ticks {overlap[0]} through {overlap[-1]}")
        return 0
    if first is not None:
        print(f"first different confirmed tick {first}")
    if field is not None:
        print(f"first different snapshot field {field} at tick {a['tick']}: {sa.get(field)} / {sb.get(field)}")
    return 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    args = parser.parse_args()
    try:
        return compare(args.left, args.right)
    except (OSError, ValueError, KeyError) as error:
        print(f"network dump: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
