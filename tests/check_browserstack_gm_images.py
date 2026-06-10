import re
import os
import sys

scriptDir = os.path.abspath(os.path.dirname(__file__))

# Parse the expected GM names out of gmmain.cpp
expectedGMNames = set()
gmmainPath = os.path.join(scriptDir, "gm", "gmmain.cpp")
with open(gmmainPath, 'r') as file:
    for line in file:
        if line.strip().startswith("#define"):
            continue
        match = re.search(r'MAKE_GM\(([^\)]+)\)', line)
        if match == None:
            continue
        expectedGMNames.add(match.group(1))

# Now make the set of all gm images found in the browserstack_gms directory
# (without extensions so that they exactly match the names from the cpp file)
imgDir  = os.path.abspath(
    os.path.join(scriptDir, "..", "..", "..", "zzzgold", "browserstack_gms"))
gmImagesFound = {os.path.splitext(f)[0] for f in os.listdir(imgDir)}

missingGMImages = sorted(expectedGMNames - gmImagesFound, key=str.casefold);
extraGMImages = sorted(gmImagesFound - expectedGMNames, key=str.casefold);

if missingGMImages:
    if len(missingGMImages) == 1:
        print("There is a missing image in the zzzgold/browserstack_gms directory.", flush=True)
    else:
        print("There are missing images in the zzzgold/browserstack_gms directory.", flush=True)
    print(f"Copy the following file{'s' if len(missingGMImages) > 1 else ''} from somewhere else", flush=True)
    print("  (a good default choice of source directory is 'zzzgold/gms_and_goldens/pixel6/gl'):", flush=True)
    print("", flush=True)
    for name in missingGMImages:
        print(f"    '{name}'", flush=True)
    print("", flush=True)

if extraGMImages:
    if len(extraGMImages) == 1:
        print("There is an extra image in the zzzgold/browserstack_gms directory.", flush=True)
    else:
        print("There are extra images in the zzzgold/browserstack_gms directory.", flush=True)
    print(f"The following file{'s do' if len(extraGMImages) > 1 else ' does'} not belong to any existing gm.", flush=True)
    print("  Was a gm removed? If so remove the following:", flush=True)
    print("", flush=True)
    for name in extraGMImages:
        print(f"    '{name}'", flush=True)
    print("", flush=True)

if missingGMImages or extraGMImages:
    sys.exit(1)

print("All expected browserstack_gms images are present.")