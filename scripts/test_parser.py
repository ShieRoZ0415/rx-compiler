#!/usr/bin/env python3
"""Run the parser testcases with the correct grammar entry point.

Reads tests/parser/manifest.json, invokes target/parser_driver with the entry
recorded in each case's metadata.entry, and compares the exit code against
compilation_success (0 = accept, 1 = reject). Does not modify any testcase.
"""

import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DRIVER = ROOT / "target" / "parser_driver"
MANIFEST = ROOT / "tests" / "parser" / "manifest.json"

manifest = json.loads(MANIFEST.read_text())

stats = Counter()
fails = []

for e in manifest:
    src = ROOT / "tests" / "parser" / e["source"]
    entry = e.get("metadata", {}).get("entry", "crate")
    expect_ok = e["compilation_success"]
    r = subprocess.run([str(DRIVER), "--entry", entry, str(src)],
                       capture_output=True)
    expected = 0 if expect_ok else 1
    ok = (r.returncode == expected)
    stats[(entry, "accept" if expect_ok else "reject", "pass" if ok else "fail")] += 1
    if not ok:
        fails.append((e["source"], entry, expect_ok, r.returncode))

print(f"total={len(manifest)} failed={len(fails)}")
print()
for key in sorted(stats, key=lambda x: (x[0], x[1], x[2])):
    entry, kind, result = key
    print(f"  {entry:14} {kind:8} {result}: {stats[key]}")

if fails:
    print("\nfailures:")
    for src, entry, expect_ok, code in fails:
        want = 0 if expect_ok else 1
        print(f"  {src}  entry={entry}  expected_exit={want}  got={code}")

sys.exit(1 if fails else 0)
