#!/usr/bin/env python3
"""Regenerate the mechanical fields of Annex D from the conformance
tree. The annex is committed; this tool is never a build step.

The annex's module table carries two mechanical columns -- whether a
module is a test root and how many test bindings it declares -- plus
the suite-wide counts in D.1 and D.2. Those fields are a direct
reading of tests/conformance and drift whenever the tree changes; the
clause and coverage columns are editorial and stay as written. This
tool rewrites only the mechanical fields, inserts a row for a module
the table does not carry (with empty editorial cells, so a later edit
must fill them), and drops rows whose module is gone.

With --check nothing is written; the tool reports each drift and exits
nonzero, so a suite change that forgot the annex fails before commit.

Usage: python3 tools/gen_annex_d.py [--check]
"""

import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..")
ANNEX = os.path.join(ROOT, "docs", "annex-d-conformance.md")
CONF = os.path.join(ROOT, "tests", "conformance")
NEGATIVE = os.path.join(ROOT, "tests", "negative", "manifest.md")

TEST_BINDING = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*: test =", re.M)
NEG_ROW = re.compile(r"^\| (N\d+) \|", re.M)
MODULE_ROW = re.compile(r"^\| `([a-z0-9_]+\.eko)` \| (yes|no) \| (\d+) \| (.*)$")


def modules():
    """One (stem, root, tests) per conformance module, sorted by stem."""
    out = []
    for path in sorted(glob.glob(os.path.join(CONF, "*.eko"))):
        text = open(path, encoding="utf-8").read()
        tests = len(TEST_BINDING.findall(text))
        out.append((os.path.basename(path), "yes" if tests else "no", tests))
    return out


def negative_count():
    """The manifest's entry count: one row per Nxx identifier."""
    return len(set(NEG_ROW.findall(open(NEGATIVE, encoding="utf-8").read())))


def field(text, pattern, render):
    """Rewrites one mechanical sentence, returning (text, drifted)."""
    m = re.search(pattern, text)
    if not m:
        return text, True
    want = render(m)
    if m.group(0) == want:
        return text, False
    return text[: m.start()] + want + text[m.end():], True


def rewrite(mods, negatives):
    text = open(ANNEX, encoding="utf-8").read()
    roots = sum(1 for _, r, _ in mods if r == "yes")
    fixtures = len(mods) - roots
    total = sum(t for _, _, t in mods)
    drift = []

    for pattern, render, label in (
        (r"\d+ Eko modules: \d+ test roots and one fixture",
         lambda m: "%d Eko modules: %d test roots and %s" % (
             len(mods), roots,
             "one fixture" if fixtures == 1 else "%d fixtures" % fixtures),
         "D.1 module count"),
        (r"\| Negative entries \| `[^`]+` \| \d+ minimal programs",
         lambda m: m.group(0).rsplit("|", 1)[0] + "| %d minimal programs"
             % negatives,
         "D.1 negative count"),
        (r"The suite contains \d+ test bindings",
         lambda m: "The suite contains %d test bindings" % total,
         "D.2 test count"),
    ):
        text, moved = field(text, pattern, render)
        if moved:
            drift.append(label)

    rows = {}
    order = []
    for line in text.splitlines():
        m = MODULE_ROW.match(line)
        if m:
            rows[m.group(1)] = m.group(0)
            order.append(m.group(1))
    lines = text.splitlines()
    seen = set()
    for i, line in enumerate(lines):
        m = MODULE_ROW.match(line)
        if not m:
            continue
        stem = m.group(1)
        seen.add(stem)
        hit = next((r for r in mods if r[0] == stem), None)
        if not hit:
            drift.append("D.5 stale %s" % stem)
            lines[i] = None
            continue
        if hit[1] == m.group(2) and str(hit[2]) == m.group(3):
            continue
        drift.append("D.5 row %s" % stem)
        lines[i] = "| `%s` | %s | %d | %s" % (stem, hit[1], hit[2], m.group(4))
    lines = [l for l in lines if l is not None]
    for stem, root, tests in mods:
        if stem not in seen:
            drift.append("D.5 missing %s" % stem)
            at = next((i for i, l in enumerate(lines)
                       if MODULE_ROW.match(l)
                       and MODULE_ROW.match(l).group(1) > stem), None)
            row = "| `%s` | %s | %d |  |  |" % (stem, root, tests)
            lines.insert(at if at is not None else len(lines), row)
    return "\n".join(lines) + "\n", drift


def main():
    check = "--check" in sys.argv[1:]
    mods = modules()
    if not mods:
        sys.exit("gen_annex_d: no conformance modules found")
    text, drift = rewrite(mods, negative_count())
    if drift:
        for d in drift:
            print("gen_annex_d: drift at %s" % d)
        if check:
            sys.exit(1)
        open(ANNEX, "w", encoding="utf-8").write(text)
        print("gen_annex_d: rewrote %s" % os.path.relpath(ANNEX, ROOT))
    else:
        print("gen_annex_d: the annex matches the tree")


if __name__ == "__main__":
    main()
