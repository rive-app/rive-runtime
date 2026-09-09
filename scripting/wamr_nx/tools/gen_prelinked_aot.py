"""Compiles a wasm module into a linkable prelinked-AOT pair: an object file
whose text the native linker resolves and places in the host image, plus a
generated cpp embedding the .aot metadata container and registering both with
the runtime's prelinked registry.

The object and the container come from the same wamrc invocation flags, and
the tool refuses to emit anything unless the container's text payload is byte
identical to the object's .text, which is the property the whole scheme
rests on.
"""

import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path

AOT_SECTION_TEXT = 2

# --xip implies --disable-llvm-intrinsics: floating point lowers to
# aot_intrinsic_* helper calls and the llvm.experimental.constrained.* nodes
# behind the AArch64 strict-FP miscompile are never emitted, so these
# artifacts deliberately keep optimized backend codegen instead of the
# --codegen-opt-level=0 cap the runtime ladder applies to non-xip builds.
WAMRC_FLAGS = [
    "--xip",
    "--bounds-checks=1",
    "--target=aarch64v8",
    "--target-abi=gnu",
    # x18 is the console's reserved platform register, and the gnu triple
    # would otherwise let LLVM allocate it freely. Features require an
    # explicit cpu; generic keeps codegen comparable across bakes.
    "--cpu=generic",
    "--cpu-features=+reserve-x18",
    "--opt-level=1",
]


def run(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"command failed: {' '.join(str(c) for c in cmd)}\n"
                 f"{result.stdout}{result.stderr}")
    return result


def extract_wasm_from_riv(data: bytes) -> bytes:
    # Validation-lane heuristic; the product path is rive prelink, which
    # decodes the riv through the runtime and takes the module bytes from
    # the object model. A file with several embedded modules is ambiguous
    # here, so refuse it rather than hash the wrong span.
    start = data.find(b"\x00asm")
    if start < 0:
        sys.exit("no wasm module found in riv")
    if data.find(b"\x00asm", start + 4) != -1:
        sys.exit("multiple wasm magics in riv; extract the module "
                 "authoritatively and rerun on the .wasm")
    i = start + 8

    def uleb(pos):
        value = 0
        shift = 0
        while True:
            byte = data[pos]
            pos += 1
            value |= (byte & 0x7F) << shift
            if not byte & 0x80:
                return value, pos
            shift += 7

    while i < len(data):
        try:
            section_id, j = uleb(i)
            size, j = uleb(j)
        except IndexError:
            break
        if section_id > 13 or j + size > len(data):
            break
        i = j + size
    return data[start:i]


def aot_text_payload(aot: bytes) -> bytes:
    p = 8
    while True:
        p = (p + 3) & ~3
        if p + 8 > len(aot):
            break
        section_type, size = struct.unpack_from("<II", aot, p)
        p += 8
        if section_type == AOT_SECTION_TEXT:
            body = aot[p:p + size]
            literal_size = struct.unpack_from("<I", body, 0)[0]
            if literal_size != 0:
                sys.exit(f"unexpected literal size {literal_size}; the "
                         "prelinked loader assumes bare code")
            return body[4:]
        p += size
    sys.exit("no text section in aot container")


def strip_text_payload(aot: bytes) -> bytes:
    """Re-serializes the container with an empty text section: the linked
    object supplies that code, so embedding it again would ship every module
    twice. The runtime walker substitutes the linked text for section 2 and
    never reads the container's copy."""
    out = bytearray(aot[:8])
    p = 8
    while True:
        p = (p + 3) & ~3
        if p + 8 > len(aot):
            break
        section_type, size = struct.unpack_from("<II", aot, p)
        p += 8
        body = aot[p:p + size]
        p += size
        while len(out) % 4:
            out.append(0)
        if section_type == AOT_SECTION_TEXT:
            out += struct.pack("<II", section_type, 0)
        else:
            out += struct.pack("<II", section_type, size) + body
    return bytes(out)


def fnv1a64(data: bytes) -> int:
    value = 0xCBF29CE484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", required=True,
                        help="identifier for symbols and file names")
    parser.add_argument("--riv", type=Path, required=True)
    parser.add_argument("--wamrc", type=Path, required=True)
    parser.add_argument("--objcopy", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", args.name):
        sys.exit(f"--name '{args.name}' is not a valid identifier component")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    wasm = extract_wasm_from_riv(args.riv.read_bytes())
    wasm_path = args.out_dir / f"{args.name}.wasm"
    wasm_path.write_bytes(wasm)

    object_path = args.out_dir / f"{args.name}.o"
    aot_path = args.out_dir / f"{args.name}.aot"
    run([args.wamrc, *WAMRC_FLAGS, "--format=object",
         "-o", object_path, wasm_path])
    run([args.wamrc, *WAMRC_FLAGS, "-o", aot_path, wasm_path])

    text_bin = args.out_dir / f"{args.name}.text.bin"
    run([args.objcopy, "-O", "binary", "--only-section=.text",
         object_path, text_bin])
    object_text = text_bin.read_bytes()
    container_text = aot_text_payload(aot_path.read_bytes())
    if object_text != container_text:
        sys.exit(f"{args.name}: object .text ({len(object_text)} bytes) != "
                 f"container text ({len(container_text)} bytes); "
                 "wamrc emitted divergent code")

    # Locals cannot collide across modules; the one global marks the text
    # start for the generated registration.
    text_symbol = f"rive_prelinked_{args.name}_text"
    run([args.objcopy, "-w", "--localize-symbol=*", object_path])
    run([args.objcopy, f"--add-symbol={text_symbol}=.text:0,global",
         object_path])

    key = fnv1a64(wasm)
    aot = strip_text_payload(aot_path.read_bytes())
    lines = []
    for i in range(0, len(aot), 16):
        chunk = ", ".join(f"0x{b:02x}" for b in aot[i:i + 16])
        lines.append(f"    {chunk},")
    array = "\n".join(lines)

    cpp = f"""/*
 * Generated by gen_prelinked_aot.py; do not edit.
 * module: {args.name}  key: {key:016x}  text: {len(container_text)} bytes
 */

#include "rive/wasm/prelinked_aot.hpp"

extern "C" const uint8_t {text_symbol}[];

namespace
{{

alignas(8) const uint8_t kAotContainer[] = {{
{array}
}};

const bool kRegistered = []() {{
    rive::registerPrelinkedAotModule({{
        0x{key:016x}ull,
        kAotContainer,
        sizeof(kAotContainer),
        {text_symbol},
        {len(container_text)},
        {len(wasm)},
    }});
    return true;
}}();

}} // namespace
"""
    cpp_path = args.out_dir / f"{args.name}_prelinked.cpp"
    cpp_path.write_text(cpp)
    print(f"{args.name}: key {key:016x}, text {len(container_text)} bytes, "
          f"metadata {len(aot)} bytes -> {cpp_path.name}, {object_path.name}")


if __name__ == "__main__":
    main()
