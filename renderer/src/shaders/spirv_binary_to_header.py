# This is a little utility to take a SPIR-V binary file and turn it into a header file
import struct
import sys

if (len(sys.argv) != 4):
    print("Usage: python binary_to_header.py <input_binary_file_path> <output_header_file_path> <array_name>")
    sys.exit(1)

inPath = sys.argv[1]
outPath = sys.argv[2]
arrayName = sys.argv[3]

# Read the entire file in as binary then convert it into an array of integers
with open(inPath, "rb") as file:
    binaryData = file.read()
if (len(binaryData) % 4 != 0):
    printf("ERROR: Shader binary length was expected to be a multiple of 32 bits")
    sys.exit(1)
data = struct.unpack(f"<{len(binaryData)//4}I", binaryData)

with open(outPath, "w") as file:
    file.write("#pragma once\n\n")
    file.write(f"const uint32_t {arrayName}[] = {{")
    for i,val in enumerate(data):
        # Add newlines (and indents) every 8 elements
        if (i % 8 == 0):
            file.write("\n    ")
        file.write(f"{val:#010x},")
    file.write("\n};\n")