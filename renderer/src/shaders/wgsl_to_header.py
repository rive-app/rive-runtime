# Embed a WGSL source file as a C++ string literal in a header file, plus emit a
# per-specialization-index bool array showing which specialization constants
# survived naga's reachability pass. The C++ side iterates the array to set only
# overrides actually present in the shader (WebGPU doesn't allow overrides that
# aren't reachable from the main entry point, so a fixed all-true list would
# fail validation).
#
# By default, the WGSL source is minified (whitespace collapsed and naga's
# auto-generated locals shortened). Pass --raw to leave the source untouched,
# used when the build sets --raw_shaders.
import argparse
import os
import re
import sys

# Alphabet used to generate short replacement names for naga's auto-generated
# variables. The only character we have to exclude is `_` itself, since WGSL
# reserves identifiers that begin with `__`.
SAFE_NAME_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

parser = argparse.ArgumentParser()
parser.add_argument("input")
parser.add_argument("output")
parser.add_argument("variable_name")
parser.add_argument(
    "--raw",
    action="store_true",
    help="don't minify the WGSL source",
)
args = parser.parse_args()

inPath = args.input
outPath = args.output
variableName = args.variable_name

# Label is the .wgsl filename minus the extension (e.g., "color_ramp.vert").
# Used as the WGPUShaderModuleDescriptor.label so WebGPU names the module in
# its error messages.
label = os.path.basename(inPath)
if label.endswith(".wgsl"):
    label = label[: -len(".wgsl")]

# Pull SPECIALIZATION_COUNT from the sibling constants.glsl so the emitted
# array length tracks the canonical value automatically.
scriptDir = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(scriptDir, "constants.glsl"), "r", encoding="utf-8") as f:
    constantsSrc = f.read()
countMatch = re.search(
    r"^\s*#define\s+SPECIALIZATION_COUNT\s+(\d+)", constantsSrc, re.M
)
if not countMatch:
    sys.stderr.write("ERROR: could not find SPECIALIZATION_COUNT in constants.glsl\n")
    sys.exit(1)
specializationCount = int(countMatch.group(1))

with open(inPath, "r", encoding="utf-8") as f:
    wgsl = f.read()

# WebGPU compatibility mode rejects @interpolate(flat) and
# @interpolate(flat, first).
# Since all of Rive's flat varyings are constant across the primitive, it
# doesn't matter which vertex is provoking and we can dodge this validation
# error by post-converting all flat varyings to 'either'.
wgsl = re.sub(
    r"@interpolate\(\s*flat\s*(?:,\s*first\s*)?\)",
    "@interpolate(flat, either)",
    wgsl,
)

# Minify the wgsl, unless otherwise specified.
# NOTE: This could have ideally been done with wgsl-minifier, but it doesn't
# support "enable clip_distances", so we roll out our own basic minifier.
if not args.raw:
    # Strip indentation and blank lines, then join with one space so adjacent
    # identifiers/keywords don't merge across the original line break.
    minified = []
    for line in wgsl.splitlines():
        line = line.strip()
        if line:
            minified.append(line)
    wgsl = " ".join(minified)

    # Drop whitespace around punctuation. `(` and `)` are deliberately excluded;
    # stripping them broke golden tests, likely because of WGSL's template-list
    # disambiguation interacting with adjacent `>(`.
    wgsl = re.sub(r"\s*([;:=,{}\[\]@~+\-*/<>&|^%!])\s*", r"\1", wgsl)

    # Rename naga's auto-generated variables to shorter forms. Pattern-based so
    # we only touch names that naga reliably generates — never builtins, etc.
    nagaPattern = re.compile(
        r"\b(?:_e\d+|local(?:_\d+)?|phi_\d+_|unnamed|main_\d+|_naga_\w+|gl_\w+)\b"
    )
    nagaNames = set(nagaPattern.findall(wgsl))
    counts = {
        n: len(re.findall(r"\b" + re.escape(n) + r"\b", wgsl))
        for n in nagaNames
    }

    def shortName(idx):
        # Bijective base-62 over `SAFE_NAME_CHARS`: idx 0..61 → _a.._9,
        # 62..3905 → _aa.._99, 3906.. → _aaa.., growing one char at a time
        # forever. Subtracting 1 each iteration is what makes it bijective —
        # in regular base-N, leading 'zeros' would be ambiguous (1, 01, 001
        # all mean the same number), and we'd never reach strings like `aa`.
        out = ""
        idx += 1
        while idx > 0:
            idx -= 1
            out = SAFE_NAME_CHARS[idx % len(SAFE_NAME_CHARS)] + out
            idx //= len(SAFE_NAME_CHARS)
        return "_" + out

    mapping = {}
    shortIdx = 0
    for name in sorted(nagaNames, key=lambda n: -counts[n]):
        candidate = shortName(shortIdx)
        if len(candidate) >= len(name):
            # Skip — wouldn't actually shrink. Don't burn an idx slot.
            continue
        mapping[name] = candidate
        shortIdx += 1
    if mapping:
        renamePattern = re.compile(
            r"\b(" + "|".join(re.escape(n) for n in mapping) + r")\b"
        )
        wgsl = renamePattern.sub(lambda m: mapping[m.group(0)], wgsl)

# Match "@id(N) override <Name>: <type> = <default>;" — any override that
# made it into the WGSL output. The numeric @id matches the specialization idx
# used by the Vulkan side, so the C++ array can be indexed by it directly.
usedIndices = {
    int(m.group(1))
    for m in re.finditer(r"@id\((\d+)\)\s+override\s+\w+\s*:", wgsl)
}
for idx in usedIndices:
    if idx >= specializationCount:
        sys.stderr.write(
            f"ERROR: WGSL '{label}' uses @id({idx}), out of range for "
            f"SPECIALIZATION_COUNT={specializationCount}\n"
        )
        sys.exit(1)

# Use a raw string literal so we don't have to escape anything. Bail out if
# the chosen delimiter happens to appear in the source — extremely unlikely
# for valid WGSL, but worth catching to avoid silently corrupting the embed.
delimiter = "WGSL"
if f'){delimiter}"' in wgsl:
    sys.stderr.write(
        f'ERROR: WGSL source contains the raw-string terminator "){delimiter}\\"".\n'
    )
    sys.exit(1)

with open(outPath, "w", encoding="utf-8") as f:
    f.write("#pragma once\n\n")
    f.write("#include <array>\n\n")
    # SPECIALIZATION_COUNT lives in constants.glsl. Including it here makes
    # the generated header self-contained.
    f.write('#include "shaders/constants.glsl"\n\n')
    f.write("namespace rive::gpu::wgsl\n")
    f.write("{\n")

    # The shared Shader type, repeated in every generated header behind an
    # #ifndef guard so every translation unit sees a single definition. Per
    # the C++ ODR, repeating the same struct definition across TUs is fine
    # as long as the tokens are identical — which they are, since they all
    # come from this script.
    f.write("#ifndef RIVE_WGSL_SHADER_DEFINED\n")
    f.write("#define RIVE_WGSL_SHADER_DEFINED\n")
    f.write("struct Shader\n")
    f.write("{\n")
    f.write("    const char* source;\n")
    f.write("    std::array<bool, SPECIALIZATION_COUNT> usedOverrides;\n")
    f.write("    const char* label;\n")
    f.write("};\n")
    f.write("#endif\n\n")

    f.write(f"inline constexpr Shader {variableName} = {{\n")
    f.write(f'    .source = R"{delimiter}(\n')
    f.write(wgsl)
    f.write(f'){delimiter}",\n')
    # Same length for every shader; entry i is true iff the WGSL declares
    # @id(i). The inner braces wrap the std::array's underlying C array.
    flags = ["true" if i in usedIndices else "false" for i in range(specializationCount)]
    f.write("    .usedOverrides = {{" + ", ".join(flags) + "}},\n")
    f.write(f'    .label = "{label}",\n')
    f.write("};\n")

    f.write("} // namespace wgsl\n")
