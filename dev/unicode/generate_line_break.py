#!/usr/bin/env python3
"""Generates the UAX #14 line break class table for src/text/line_break_data.cpp.

Usage:
  generate_line_break.py --download 17.0.0
  generate_line_break.py --ucd <dir with LineBreak.txt, UnicodeData.txt,
                                EastAsianWidth.txt, emoji-data.txt,
                                LineBreakTest.txt>

Writes src/text/line_break_data.cpp and the comment stripped conformance
file tests/unit_tests/assets/unicode/LineBreakTest.txt.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import urllib.request

RUNTIME = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA_OUT = os.path.join(RUNTIME, "src", "text", "line_break_data.cpp")
TEST_OUT = os.path.join(RUNTIME, "tests", "unit_tests", "assets", "unicode",
                        "LineBreakTest.txt")

UCD_FILES = {
    "LineBreak.txt": "LineBreak.txt",
    "UnicodeData.txt": "UnicodeData.txt",
    "EastAsianWidth.txt": "EastAsianWidth.txt",
    "emoji-data.txt": "emoji/emoji-data.txt",
    "LineBreakTest.txt": "auxiliary/LineBreakTest.txt",
}

# Must match LineBreakClass in include/rive/text/line_break.hpp. AI, SG, XX
# and SA are resolved by LB1 at generation time so they never appear.
CLASSES = [
    "AL", "AK", "AP", "AS", "B2", "BA", "BB", "BK", "CB", "CJ", "CL", "CM",
    "CP", "CR", "EB", "EM", "EX", "GL", "H2", "H3", "HH", "HL", "HY", "ID",
    "IN", "IS", "JL", "JT", "JV", "LF", "NL", "NS", "NU", "OP", "PO", "PR",
    "QU", "RI", "SP", "SY", "VF", "VI", "WJ", "ZW", "ZWJ",
]

# Must match the LineBreakFlags bits in line_break.hpp.
FLAG_EAST_ASIAN = 1
FLAG_PI = 2
FLAG_PF = 4
FLAG_DOTTED_CIRCLE = 8
FLAG_PICTOGRAPHIC_CN = 16

MAX_CP = 0x110000


def download(version, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for name, path in UCD_FILES.items():
        dest = os.path.join(out_dir, name)
        if os.path.exists(dest):
            continue
        url = f"https://www.unicode.org/Public/{version}/ucd/{path}"
        print(f"fetching {url}")
        urllib.request.urlretrieve(url, dest)


def parse_ranges(path, value_index=1):
    """Yields (start, end, value) for lines like 'XXXX..YYYY ; VALUE'."""
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            fields = [x.strip() for x in line.split(";")]
            cps = fields[0]
            if ".." in cps:
                a, b = cps.split("..")
            else:
                a = b = cps
            yield int(a, 16), int(b, 16), fields[value_index]


def fill(table, path, default, value_index=1, only=None):
    for a, b, v in parse_ranges(path, value_index):
        if only is not None and v != only:
            continue
        for cp in range(a, b + 1):
            table[cp] = v


def parse_general_category(path):
    gc = ["Cn"] * MAX_CP
    first = None
    with open(path, encoding="utf-8") as f:
        for line in f:
            fields = line.rstrip("\n").split(";")
            cp = int(fields[0], 16)
            name = fields[1]
            cat = fields[2]
            if name.endswith(", First>"):
                first = cp
                continue
            if name.endswith(", Last>"):
                for c in range(first, cp + 1):
                    gc[c] = cat
                first = None
                continue
            gc[cp] = cat
    return gc


def build_props(ucd):
    lb = ["XX"] * MAX_CP
    fill(lb, os.path.join(ucd, "LineBreak.txt"), "XX")
    eaw = ["N"] * MAX_CP
    fill(eaw, os.path.join(ucd, "EastAsianWidth.txt"), "N")
    pict = [False] * MAX_CP
    for a, b, v in parse_ranges(os.path.join(ucd, "emoji-data.txt")):
        if v == "Extended_Pictographic":
            for cp in range(a, b + 1):
                pict[cp] = True
    gc = parse_general_category(os.path.join(ucd, "UnicodeData.txt"))

    props = []
    for cp in range(MAX_CP):
        cls = lb[cp]
        cat = gc[cp]
        if cls in ("AI", "SG", "XX"):
            cls = "AL"
        elif cls == "SA":
            cls = "CM" if cat in ("Mn", "Mc") else "AL"
        flags = 0
        if eaw[cp] in ("F", "W", "H"):
            flags |= FLAG_EAST_ASIAN
        if cat == "Pi":
            flags |= FLAG_PI
        if cat == "Pf":
            flags |= FLAG_PF
        if cp == 0x25CC:
            flags |= FLAG_DOTTED_CIRCLE
        if pict[cp] and cat == "Cn":
            flags |= FLAG_PICTOGRAPHIC_CN
        props.append((cls, flags))
    return props


def build_trie(values, default):
    """Picks the smallest three level layout: a top index by cp >> shift1,
    middle blocks indexing data blocks of 1 << shift2 values."""
    high = MAX_CP
    while high > 0 and values[high - 1] == default:
        high -= 1
    best = None
    for shift2 in (4, 5, 6, 7):
        for shift1 in range(shift2 + 3, 13):
            block = 1 << shift2
            group = 1 << shift1
            high_start = (high + group - 1) // group * group
            data_blocks = {}
            mid_blocks = {}
            top = []
            for gstart in range(0, high_start, group):
                mid = []
                for start in range(gstart, gstart + group, block):
                    chunk = tuple(values[start:start + block])
                    if chunk not in data_blocks:
                        data_blocks[chunk] = len(data_blocks)
                    mid.append(data_blocks[chunk])
                mid = tuple(mid)
                if mid not in mid_blocks:
                    mid_blocks[mid] = len(mid_blocks)
                top.append(mid_blocks[mid])
            top_width = 1 if len(mid_blocks) <= 256 else 2
            mid_width = 1 if len(data_blocks) <= 256 else 2
            size = (len(top) * top_width +
                    len(mid_blocks) * (group >> shift2) * mid_width +
                    len(data_blocks) * block)
            if best is None or size < best["size"]:
                best = {
                    "shift1": shift1,
                    "shift2": shift2,
                    "high_start": high_start,
                    "top": top,
                    "top_width": top_width,
                    "mid": [v for k in mid_blocks.keys() for v in k],
                    "mid_width": mid_width,
                    "data": [v for k in data_blocks.keys() for v in k],
                    "size": size,
                    "counts": (len(top), len(mid_blocks), len(data_blocks)),
                }
    return best


def format_array(values, per_line=16):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append("    " + ", ".join(str(v) for v in values[i:i + per_line]) + ",")
    return "\n".join(lines)


def write_data(props, version):
    ext = {}
    ext_list = []
    values = []
    for p in props:
        if p not in ext:
            ext[p] = len(ext_list)
            ext_list.append(p)
        values.append(ext[p])
    assert len(ext_list) <= 256
    default = ext[("AL", 0)]
    trie = build_trie(values, default)
    top_type = "uint8_t" if trie["top_width"] == 1 else "uint16_t"
    mid_type = "uint8_t" if trie["mid_width"] == 1 else "uint16_t"

    out = []
    out.append("// Generated by dev/unicode/generate_line_break.py from Unicode "
               f"{version}. Do not edit.\n")
    out.append("#include \"rive/text/line_break.hpp\"\n\n")
    out.append("namespace rive\n{\n")
    out.append(f"extern const uint32_t kLineBreakHighStart = 0x{trie['high_start']:X};\n")
    out.append(f"extern const uint32_t kLineBreakTopShift = {trie['shift1']};\n")
    out.append(f"extern const uint32_t kLineBreakBlockShift = {trie['shift2']};\n")
    out.append(f"extern const uint8_t kLineBreakDefault = {default};\n\n")
    out.append(f"extern const LineBreakProps kLineBreakExtClasses[{len(ext_list)}] = {{\n")
    for cls, flags in ext_list:
        out.append(f"    {{LineBreakClass::{cls}, {flags}}},\n")
    out.append("};\n\n")
    out.append(f"extern const {top_type} kLineBreakTop[{len(trie['top'])}] = {{\n")
    out.append(format_array(trie["top"]))
    out.append("\n};\n\n")
    out.append(f"extern const {mid_type} kLineBreakMid[{len(trie['mid'])}] = {{\n")
    out.append(format_array(trie["mid"]))
    out.append("\n};\n\n")
    out.append(f"extern const uint8_t kLineBreakData[{len(trie['data'])}] = {{\n")
    out.append(format_array(trie["data"], 24))
    out.append("\n};\n")
    out.append("} // namespace rive\n")
    with open(DATA_OUT, "w", encoding="utf-8") as f:
        f.write("".join(out))
    if shutil.which("clang-format"):
        subprocess.run(["clang-format", "-i", DATA_OUT], check=True)
    top, mid, data = trie["counts"]
    print(f"{len(ext_list)} extended classes, shifts {trie['shift1']}/"
          f"{trie['shift2']}, {top} top, {mid} mid, {data} data blocks, "
          f"high start 0x{trie['high_start']:X}, "
          f"{trie['size'] / 1024:.1f} KB of table data")


def write_test(ucd):
    os.makedirs(os.path.dirname(TEST_OUT), exist_ok=True)
    count = 0
    with open(os.path.join(ucd, "LineBreakTest.txt"), encoding="utf-8") as src, \
            open(TEST_OUT, "w", encoding="utf-8") as dst:
        for line in src:
            if line.startswith("# LineBreakTest") or line.startswith("# Date"):
                dst.write(line)
                continue
            body = line.split("#", 1)[0].strip()
            if body:
                dst.write(body + "\n")
                count += 1
    print(f"{count} conformance cases written to {TEST_OUT}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--download", metavar="VERSION",
                        help="fetch the UCD files for this Unicode version")
    parser.add_argument("--ucd", help="directory holding the UCD files")
    args = parser.parse_args()
    ucd = args.ucd
    version = None
    if args.download:
        version = args.download
        ucd = ucd or os.path.join(os.path.dirname(__file__), "ucd", version)
        download(version, ucd)
    if not ucd:
        parser.error("--download or --ucd required")
    with open(os.path.join(ucd, "LineBreak.txt"), encoding="utf-8") as f:
        m = re.match(r"# LineBreak-([\d.]+)\.txt", f.readline())
        version = m.group(1) if m else version or "unknown"
    props = build_props(ucd)
    write_data(props, version)
    write_test(ucd)


if __name__ == "__main__":
    sys.exit(main())
