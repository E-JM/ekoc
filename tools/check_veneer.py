#!/usr/bin/env python3
"""Check the veneer prototypes of the spec and the header against each
other. The signatures are normative (Clause 10.7.1), rt/ekoveneer.h is
their single source of truth, and this tool fails when the two lists
drift. The files are committed; this tool is never a build step.

Usage: python3 tools/check_veneer.py [--check]
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(ROOT, "rt", "ekoveneer.h")
SPEC = os.path.join(ROOT, "docs", "10-native-interface", "04-embedding.md")


def strip_comments(text):
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def header_signatures():
    text = strip_comments(open(HEADER).read())
    sigs = {}
    for m in re.finditer(r"^(eko_\w+|void)\s+(eko_\w+)\s*\(([^;]*)\);",
                         text, flags=re.M | re.S):
        ret, name, params = m.group(1), m.group(2), m.group(3)
        params = " ".join(params.split()).replace(", ", ",")
        sigs[name] = f"{ret} {name}({params})"
    return sigs


def spec_signatures():
    text = open(SPEC).read()
    blocks = re.findall(r"```c\n(.*?)```", text, flags=re.S)
    sigs = {}
    for block in blocks:
        for line in block.splitlines():
            line = " ".join(line.split())
            m = re.match(r"^(eko_\w+|void)\s+(eko_\w+)\((.*?)\);$", line)
            if not m:
                continue
            ret, name, params = m.group(1), m.group(2), m.group(3)
            params = params.replace(", ", ",")
            if name in sigs:
                raise SystemExit(
                    f"{SPEC}: `{name}` is declared twice in the prototype "
                    "list")
            sigs[name] = f"{ret} {name}({params})"
    return sigs


def main():
    header = header_signatures()
    spec = spec_signatures()
    drift = 0
    for name in sorted(set(header) | set(spec)):
        if name not in header:
            print(f"{SPEC}: `{name}` has no prototype in rt/ekoveneer.h")
            drift = 1
        elif name not in spec:
            print(f"{HEADER}: `{name}` is absent from the Clause 10.7.1 "
                  "prototype list")
            drift = 1
        elif header[name] != spec[name]:
            print(f"`{name}` differs:\n  header: {header[name]}"
                  f"\n  spec:   {spec[name]}")
            drift = 1
    if not drift:
        print(f"veneer: {len(header)} prototypes agree")
    return drift


if __name__ == "__main__":
    sys.exit(main())
