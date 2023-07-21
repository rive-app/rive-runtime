"""Generates pls_renames.h from a list of XML files

Parses all files as XML from sys.argv that have an extension of .xml or .o.
Assumes standard `castxml` output in the XML format.

Gathers all symbols from within the 'pls' or 'pathutils' namespace and renames
them to 'r_<count>'.

Writes pls_renames.h with #defines for each rename.
"""

import os
import re
import sys
import xml.etree.ElementTree as XML

unrenameable = {
    'Make', 'addPath', 'clearColor', 'clipPath', 'close', 'color0', 'cubicTo', 'drawImage',
    'drawPath', 'flush', 'get', 'height', 'id', 'join', 'lineTo', 'loadAction',
    'makeEmptyRenderPath', 'makeRenderPaint', 'makeRenderPath', 'moveTo', 'path', 'pixelFormat',
    'pop', 'pop_front', 'push', 'push_back', 'renderTarget', 'restore', 'rewind', 'save',
    'setColor', 'transform', 'width', 'begin', 'end', 'reset'}
swizzle_pattern = re.compile(r"^([xyzw]{1,4}|[rgba]{1,4}|[stpq]{1,4})$")
private_symbols = set()

def add_private_symbol(name):
    idx = name.find('<')
    if idx != -1:
        basename = name[:idx] # strip out template qualifiers.
    else:
        basename = name
    if basename in unrenameable:
        return # can't rename this symbol.
    if swizzle_pattern.match(basename):
        return # can't rename swizzles.
    private_symbols.add(basename)

def parse_xml(xmlfile):
    print(f'Parsing xml file {xmlfile}')
    tree = XML.parse(xmlfile)
    root = tree.getroot()

    # find the "pls", "pathutils", anonymous, and child namespace/class/struct/enum ids.
    private_namespaces = set();
    changed = True
    while changed:
        changed = False
        for node in root:
            if not node.tag in ['Namespace', 'Class', 'Struct', 'Enumeration']:
                continue
            if node.attrib['id'] in private_namespaces:
                continue # already added this namespace.
            name = node.attrib['name'] if 'name' in node.attrib else ''
            if ((node.tag == 'Namespace' and name in ['pls', 'pathutils', '']) or
                ('context' in node.attrib and node.attrib['context'] in private_namespaces)):
                print(f'  Renaming symbols in context "{name}" ({node.attrib["id"]}).')
                private_namespaces.add(node.attrib['id'])
                if name != '':
                    add_private_symbol(node.attrib['name'])
                changed = True

    # find all symbols to rename.
    for node in root.iter('*'):
        if not 'name' in node.attrib or node.attrib['name'] == '':
            continue
        if not 'context' in node.attrib:
            continue
        if node.tag in ['OperatorFunction', 'OperatorMethod']:
            continue # can't rename operators.
        if node.attrib['context'] in private_namespaces:
            add_private_symbol(node.attrib['name'])

for arg in sys.argv:
    if arg.endswith('.xml') or arg.endswith('.o'):
        parse_xml(arg)

out = open(os.path.join(os.path.dirname(sys.argv[0]), 'pls_renames.h'), 'w', newline='\n')
count = 0
for name in sorted(private_symbols):
    out.write(f'#define {name} r_{count:x}\n')
    count = count + 1
