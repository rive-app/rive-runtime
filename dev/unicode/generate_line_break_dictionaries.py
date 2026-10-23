#!/usr/bin/env python3
"""Generates the word lists that break lines inside scripts written without
spaces (UAX #14 class SA) into src/text/line_break_dict_<language>.cpp.

Usage:
  generate_line_break_dictionaries.py --download release-78.1
  generate_line_break_dictionaries.py --icu <dir with thaidict.txt,
                                             laodict.txt, khmerdict.txt,
                                             burmesedict.txt>

Each language compiles in only with its WITH_RIVE_TEXT_DICT_<LANGUAGE>
define, set by the with_rive_text_dict_<language> premake option. The word
lists are ICU's (data/brkitr/dictionaries), stored as a minimized trie, or
with WITH_RIVE_TEXT_DICT_COMPRESSED as the deflated word list the runtime
rebuilds that trie from on first use.
"""

import argparse
import os
import shutil
import subprocess
import sys
import urllib.request
import zlib

RUNTIME = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC_DIR = os.path.join(RUNTIME, "src", "text")

# ranges are the codepoints LineBreak.txt assigns SA in each script, so
# digits and punctuation keep their own classes. no_break_before and
# no_break_after keep a syllable whole when a word isn't in the list: vowels
# written before their consonant, vowels and marks that follow it without
# being combining marks, and the signs that stack the next consonant.
# suffixes (repetition and abbreviation signs) can't start a word either,
# they belong to the word before them. coda_marks make the consonant they directly follow end the previous
# syllable (killers, asat, virama), so no break may precede that consonant.
# Only coda_skip marks may sit between them. Thai and Lao silence a consonant
# together with its above or below vowel (the i in di + THANTHAKHAT), while
# a Burmese vowel sign makes asat part of the vowel instead, like aw.
LANGUAGES = {
    "thai": {
        "file": "thaidict.txt",
        "ranges": [(0x0E01, 0x0E3A), (0x0E40, 0x0E4E)],
        "no_break_before": [0x0E30, 0x0E32, 0x0E33, 0x0E45],
        "no_break_after": [0x0E40, 0x0E41, 0x0E42, 0x0E43, 0x0E44],
        "suffixes": [0x0E2F, 0x0E46],
        "coda_marks": [0x0E4C],
        # Above and below vowels.
        "coda_skip": [0x0E31, 0x0E34, 0x0E35, 0x0E36, 0x0E37, 0x0E38, 0x0E39,
                      0x0E3A, 0x0E47],
    },
    "lao": {
        "file": "laodict.txt",
        "ranges": [(0x0E81, 0x0ECE), (0x0EDC, 0x0EDF)],
        "no_break_before": [0x0EB0, 0x0EB2, 0x0EB3],
        "no_break_after": [0x0EC0, 0x0EC1, 0x0EC2, 0x0EC3, 0x0EC4],
        "suffixes": [0x0EAF, 0x0EC6],
        "coda_marks": [0x0ECC],
        # Above and below vowels.
        "coda_skip": [0x0EB1, 0x0EB4, 0x0EB5, 0x0EB6, 0x0EB7, 0x0EB8, 0x0EB9,
                      0x0EBA, 0x0EBB],
    },
    "khmer": {
        "file": "khmerdict.txt",
        "ranges": [(0x1780, 0x17D3), (0x17D7, 0x17D7), (0x17DC, 0x17DD)],
        "no_break_before": [],
        "no_break_after": [0x17D2],
        "suffixes": [0x17D7],
        # Not viriam: ICU's list keeps consonant + viriam as words of their own.
        "coda_marks": [0x17CB, 0x17CD],
        "coda_skip": [],
    },
    "burmese": {
        "file": "burmesedict.txt",
        "ranges": [(0x1000, 0x103F), (0x1050, 0x108F), (0x109A, 0x109F),
                   (0xA9E0, 0xA9EF), (0xA9FA, 0xA9FE), (0xAA60, 0xAA7F)],
        "no_break_before": [],
        "no_break_after": [0x1039],
        "suffixes": [],
        "coda_marks": [0x1039, 0x103A],
        # The dot below sorts before asat in canonical order.
        "coda_skip": [0x1037],
    },
}

# Edge layout, must match line_break_dictionary.hpp.
SYMBOL_BITS = 7
LAST_EDGE = 1 << 7
WORD_END = 1 << 8
TARGET_SHIFT = 9


def download(tag, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for lang in LANGUAGES.values():
        dest = os.path.join(out_dir, lang["file"])
        if os.path.exists(dest):
            continue
        url = ("https://raw.githubusercontent.com/unicode-org/icu/"
               f"{tag}/icu4c/source/data/brkitr/dictionaries/{lang['file']}")
        print(f"fetching {url}")
        urllib.request.urlretrieve(url, dest)


def read_words(path):
    """Returns the leading license comment and the sorted word list."""
    header = []
    words = set()
    with open(path, encoding="utf-8-sig") as f:
        for line in f:
            line = line.strip()
            if line.startswith("#"):
                # Later comments are commented out words.
                if not words:
                    header.append(" ".join(line.lstrip("#").split()))
                continue
            # Some lists carry a tab separated value after the word.
            word = line.split()[0] if line else ""
            if word:
                words.add(word)
    return [h for h in header if h], sorted(words)


def build_edges(words, alphabet):
    """Builds a minimized trie as a flat edge list. Each state's edges are
    contiguous and sorted by symbol, the root's start at 0, and an edge
    points at its target's first edge (0 when the target has none)."""
    symbol = {c: i for i, c in enumerate(alphabet)}
    trie = {}
    for word in words:
        node = trie
        for i, c in enumerate(word):
            child = node.setdefault(symbol[c], [False, {}])
            if i == len(word) - 1:
                child[0] = True
            node = child[1]

    # Merge identical subtrees. A state is its sorted (symbol, word end,
    # target) edges, so the empty state is the shared leaf.
    states = {}
    order = []

    def intern(node):
        key = tuple(sorted((s, end, intern(child))
                           for s, (end, child) in node.items()))
        if key not in states:
            states[key] = len(order)
            order.append(key)
        return states[key]

    root = intern(trie)

    # Lay states out breadth first from the root so it lands at edge 0.
    first_edge = {}
    layout = []
    queue = [root]
    seen = {root}
    edge_count = 0
    while queue:
        state = queue.pop(0)
        edges = order[state]
        if not edges:
            continue
        first_edge[state] = edge_count
        layout.append(state)
        edge_count += len(edges)
        for _, _, target in edges:
            if target not in seen:
                seen.add(target)
                queue.append(target)

    out = []
    for state in layout:
        edges = order[state]
        for i, (s, end, target) in enumerate(edges):
            value = s | (first_edge.get(target, 0) << TARGET_SHIFT)
            if end:
                value |= WORD_END
            if i == len(edges) - 1:
                value |= LAST_EDGE
            out.append(value)
    assert first_edge[root] == 0
    assert edge_count < (1 << (32 - TARGET_SHIFT))
    return out


def check(words, alphabet, edges):
    """Walks every word back out of the edge list."""
    symbol = {c: i for i, c in enumerate(alphabet)}
    for word in words:
        state = 0
        end = False
        for i, c in enumerate(word):
            if i > 0 and state == 0:
                raise AssertionError(f"{word} runs past a leaf")
            s = symbol[c]
            e = state
            while True:
                if edges[e] & ((1 << SYMBOL_BITS) - 1) == s:
                    break
                assert not edges[e] & LAST_EDGE, f"{word} missing"
                e += 1
            end = bool(edges[e] & WORD_END)
            state = edges[e] >> TARGET_SHIFT
        assert end, f"{word} not marked as a word"


def checksum(edges):
    """FNV-1a over the edge values, must match line_break_dictionary.cpp."""
    h = 2166136261
    for e in edges:
        h = ((h ^ e) * 16777619) & 0xFFFFFFFF
    return h


def compress_words(words, alphabet):
    """Front codes the sorted words, each as the length it shares with the
    previous word, its remaining symbols plus one, and a 0, then deflates
    them (raw RFC 1951). The runtime rebuilds the same trie from them."""
    symbol = {c: i for i, c in enumerate(alphabet)}
    coded = bytearray()
    prev = ""
    for word in words:
        shared = 0
        while (shared < min(len(word), len(prev)) and
               word[shared] == prev[shared]):
            shared += 1
        coded.append(shared)
        coded += bytes(symbol[c] + 1 for c in word[shared:])
        coded.append(0)
        prev = word
    deflate = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    return deflate.compress(bytes(coded)) + deflate.flush(), len(coded)


def format_array(values, per_line):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append("    " + ", ".join(values[i:i + per_line]) + ",")
    return "\n".join(lines)


def hex_list(values):
    return [f"0x{v:04X}" for v in values]


def write_language(name, lang, header, words, tag):
    alphabet = sorted({c for w in words for c in w})
    assert len(alphabet) <= 1 << SYMBOL_BITS
    edges = build_edges(words, alphabet)
    check(words, alphabet, edges)

    title = name.capitalize()
    define = f"WITH_RIVE_TEXT_DICT_{name.upper()}"
    prefix = f"k{title}"
    ranges = [v for r in lang["ranges"] for v in r]

    def span(array, values):
        return f"Span<const uint32_t>({array}, {len(values)})" \
            if values else "Span<const uint32_t>()"

    out = []
    out.append("// Generated by dev/unicode/generate_line_break_dictionaries.py "
               f"from ICU {tag}\n// {lang['file']}. Do not edit.\n//\n")
    for line in header:
        out.append(f"// {line}\n")
    out.append("\n#include \"rive/text/line_break_dictionary.hpp\"\n\n")
    out.append(f"#ifdef {define}\n\n")
    out.append("namespace rive\n{\n")
    out.append("// clang-format off\n")
    out.append(f"static const uint32_t {prefix}Ranges[{len(ranges)}] = {{\n")
    out.append(format_array(hex_list(ranges), 8))
    out.append("\n};\n\n")
    out.append(f"static const uint32_t {prefix}Alphabet[{len(alphabet)}] = {{\n")
    out.append(format_array(hex_list(ord(c) for c in alphabet), 8))
    out.append("\n};\n\n")
    for key, array in (("no_break_before", "NoBreakBefore"),
                       ("no_break_after", "NoBreakAfter"),
                       ("suffixes", "Suffixes"),
                       ("coda_marks", "CodaMarks"),
                       ("coda_skip", "CodaSkip")):
        if lang[key]:
            out.append(f"static const uint32_t {prefix}{array}"
                       f"[{len(lang[key])}] = {{\n")
            out.append(format_array(hex_list(lang[key]), 8))
            out.append("\n};\n\n")
    compressed, coded_size = compress_words(words, alphabet)
    out.append("#ifdef WITH_RIVE_TEXT_DICT_COMPRESSED\n")
    out.append(f"static const uint8_t {prefix}Words[{len(compressed)}] = {{\n")
    out.append(format_array([str(b) for b in compressed], 16))
    out.append("\n};\n")
    out.append("#else\n")
    out.append(f"static const uint32_t {prefix}Edges[{len(edges)}] = {{\n")
    out.append(format_array([str(e) for e in edges], 10))
    out.append("\n};\n")
    out.append("#endif\n")
    out.append("// clang-format on\n\n")
    out.append(f"extern const LineBreakDictionary kLineBreakDictionary{title} = {{\n")
    out.append(f"    {span(prefix + 'Ranges', ranges)},\n")
    out.append(f"    {span(prefix + 'Alphabet', alphabet)},\n")
    out.append(f"    {span(prefix + 'NoBreakBefore', lang['no_break_before'])},\n")
    out.append(f"    {span(prefix + 'NoBreakAfter', lang['no_break_after'])},\n")
    out.append(f"    {span(prefix + 'Suffixes', lang['suffixes'])},\n")
    out.append(f"    {span(prefix + 'CodaMarks', lang['coda_marks'])},\n")
    out.append(f"    {span(prefix + 'CodaSkip', lang['coda_skip'])},\n")
    out.append("#ifdef WITH_RIVE_TEXT_DICT_COMPRESSED\n")
    out.append(f"    Span<const uint8_t>({prefix}Words, {len(compressed)}),\n")
    out.append(f"    {coded_size},\n")
    out.append("#else\n")
    out.append(f"    {span(prefix + 'Edges', edges)},\n")
    out.append("#endif\n")
    out.append(f"    {len(edges)},\n")
    out.append(f"    0x{checksum(edges):08X},\n")
    out.append("};\n")
    out.append("} // namespace rive\n\n")
    out.append(f"#endif\n")

    path = os.path.join(SRC_DIR, f"line_break_dict_{name}.cpp")
    with open(path, "w", encoding="utf-8") as f:
        f.write("".join(out))
    if shutil.which("clang-format"):
        subprocess.run(["clang-format", "-i", path], check=True)
    print(f"{name}: {len(words)} words, {len(alphabet)} symbols, "
          f"{len(edges)} edges, {len(edges) * 4 / 1024:.1f} KB, "
          f"{len(compressed) / 1024:.1f} KB compressed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--download", metavar="TAG",
                        help="fetch the dictionaries from this ICU release "
                             "tag, e.g. release-78.1")
    parser.add_argument("--icu", help="directory holding the dictionaries")
    args = parser.parse_args()
    icu = args.icu
    tag = "unknown"
    if args.download:
        tag = args.download
        icu = icu or os.path.join(os.path.dirname(__file__), "icu", tag)
        download(tag, icu)
    if not icu:
        parser.error("--download or --icu required")
    tag = tag.removeprefix("release-")
    for name, lang in LANGUAGES.items():
        header, words = read_words(os.path.join(icu, lang["file"]))
        write_language(name, lang, header, words, tag)


if __name__ == "__main__":
    sys.exit(main())
