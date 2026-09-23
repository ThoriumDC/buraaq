#!/usr/bin/env python3
"""Drop duplicate LLVM declare lines (same text) so fixpoint link succeeds."""
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8", errors="replace") as f:
    lines = f.readlines()
seen: set[str] = set()
out: list[str] = []
for line in lines:
    stripped = line.rstrip("\n")
    if stripped.startswith("declare "):
        if stripped in seen:
            continue
        seen.add(stripped)
    out.append(line)
with open(path, "w", encoding="utf-8") as f:
    f.writelines(out)
