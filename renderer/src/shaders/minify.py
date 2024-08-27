import argparse
import glob
import os
import ply.lex as lex
import re
import sys
from collections import defaultdict

parser = argparse.ArgumentParser(description="""
Process a batch of .glsl files. Minify and export them to C++ strings.

Performs the following transformations:
    * Strip comments.
    * Strip whitespace.
    * Strip unused #defines.
    * Rename stpq and rgba swizzles to xyzw.
    * Rename variables.
        - No new name begins with the '_' character, so internal code can begin names with '_'
          without fear of renaming collisions.
        - GLSL keywords and builtins are not renamed.
        - Tokens beginning with '@' have their new name exported to a header file.
        - Tokens beginning with '$' are not renamed, with the exception of removing the leading '$'.

"file.glsl" gets exported to:
    * "outdir/file.exports.h", with #defines for the rewritten names of each identifier that began
      with '@' in the original shader.
    * "outdir/file.glsl.cpp", with a global `const char file_glsl[]` in the rive::glsl
      namespace that contains the minified shader code. This variable is intentionally declared as a
      global in order to generate a link error if the user includes the string more than once in
      the build process.
    * "outdir/file.minified.glsl" for offline compiling, with all variables renamed except for
      exported #defines names (since the offline compiling process will set those defines).
""")
parser.add_argument("files", type=str, nargs="+", help="a list of glsl files")
parser.add_argument("-o", "--outdir", required=True,
                    help="OUTPUT directory to store the header files")
parser.add_argument("-H", "--human-readable", action='store_true',
                    help="don't rename or strip out comments or whitespace")
args = parser.parse_args()

# tokens used by PLY to run lexical analysis on our glsl files.
tokens = [
    "DEFINE",
    "IFDEF",
    "DEFINED_ID",
    "TOKEN_PASTE",
    "DIRECTIVE",
    "LINE_COMMENT",
    "BLOCK_COMMENT",
    "WHITESPACE",
    "OP",
    "FLOAT",
    "HEX",
    "INT",
    "ID",
    "UNKNOWN",
]

# tracks which exported identifiers (identifiers whose @name begins with '@') are used as switches
# inside an #ifdef, #if defined(), etc.
exported_switches = set()

# counts the number of times each ID is seen, to prioritize which IDs get the shortest names
all_id_counts = defaultdict(int);
all_id_reference_counts = defaultdict(int);
def parse_id(name, exports, *, is_reference):
    all_id_counts[name] += 1
    if is_reference:
        all_id_reference_counts[name] += 1
    # identifiers that begin with '@' get exported to C++ through #defines.
    if name[0] == '@':
        exports.add(name)

# lexing functions used by PLY.
def t_DEFINE(tok):
    r"\#[ \t]*define[ \t]+(?P<id>[\@\$]?[A-Za-z_][A-Za-z0-9_]*)(?P<arglist>\((\n|[^\)])*\))?(?P<val>(((\\\n|.)(?!\/[\/\*]))*))?"
    tok.define_id = re.match(t_DEFINE.__doc__, tok.value)['id']
    arglist = re.match(t_DEFINE.__doc__, tok.value)['arglist']
    tok.define_arglist = Minifier(arglist, "", tok.lexer.exports) if arglist else None
    val = re.match(t_DEFINE.__doc__, tok.value)['val']
    tok.define_val = Minifier(val, "", tok.lexer.exports) if val else None
    parse_id(tok.define_id, tok.lexer.exports, is_reference=False)
    tok.lexer.lineno += tok.value.count('\n')
    return tok

def t_IFDEF(tok):
    r"\#[ \t]*(?P<tag>ifn?def)[ \t]+(?P<id>[\@\$]?[A-Za-z_][A-Za-z0-9_]*)"
    tok.ifdef_tag = re.match(t_IFDEF.__doc__, tok.value)['tag']
    tok.ifdef_id = re.match(t_IFDEF.__doc__, tok.value)['id']
    if tok.ifdef_id[0] == '@':
        exported_switches.add(tok.ifdef_id)
    parse_id(tok.ifdef_id, tok.lexer.exports, is_reference=True)
    return tok

def t_DEFINED_ID(tok):
    r"defined\((?P<defined_id>[\@\$]?[A-Za-z_][A-Za-z0-9_]*)\)"
    tok.defined_id = re.match(t_DEFINED_ID.__doc__, tok.value)['defined_id']
    if tok.defined_id[0] == '@':
        exported_switches.add(tok.defined_id)
    parse_id(tok.defined_id, tok.lexer.exports, is_reference=True)
    return tok

def t_TOKEN_PASTE(tok):
    r"\#\#"
    return tok

def t_DIRECTIVE(tok):
    r"\#[ \t]*(?P<val>(((\\\n|.)(?!\/[\/\*]))*))"
    val = re.match(t_DIRECTIVE.__doc__, tok.value)['val']
    tok.directive_val = Minifier(val, "", tok.lexer.exports) if val else None
    tok.lexer.lineno += tok.value.count('\n')
    return tok

def t_LINE_COMMENT(tok):
    r"//(\\\n|.)*"
    tok.lexer.lineno += tok.value.count('\n')
    return tok

def t_BLOCK_COMMENT(tok):
    r"\/\*(\*(?!\/)|[^*])*\*\/"
    tok.lexer.lineno += tok.value.count('\n')
    return tok

def t_WHITESPACE(tok):
    r"(\s|\\\r?\n)+"
    tok.lexer.lineno += tok.value.count('\n')
    return tok

def t_OP(tok):
    r"[~!%^&\*\(\)\-=+\/\[\]{}\?:\<\>\.\,|;]"
    return tok

def t_FLOAT(tok):
    r"([0-9]*\.[0-9]+|[0-9]+\.)([eE][+\-]?[0-9]+)?|([0-9]+[eE][+\-]?[0-9]+)"
    return tok

def t_HEX(tok):
    r"0x[0-9a-fA-F]+u?"
    return tok

def t_INT(tok):
    r"[0-9]+u?"
    return tok

def t_ID(tok):
    r"[\@\$]?[A-Za-z_][A-Za-z0-9_]*"
    parse_id(tok.value, tok.lexer.exports, is_reference=True)
    return tok

def t_UNKNOWN(tok):
    r"."
    return tok

def t_error(tok):
    raise Exception("Illegal character '%s' at line %d" % (tok.value[0], tok.lexer.lineno))

# identifier names that cannot be renamed
glsl_reserved = {
    "EmitStreamVertex", "EmitVertex", "EmitVertex", "EndPrimitive", "EndPrimitive",
    "EndStreamPrimitive", "abs", "abs", "abs", "acos", "acosh", "all", "allInvocations",
    "allInvocationsEqual", "any", "anyInvocation", "asin", "asinh", "atan", "atan", "atanh",
    "atomicAdd", "atomicAdd", "atomicAnd", "atomicAnd", "atomicCompSwap", "atomicCompSwap",
    "atomicCounter", "atomicCounterAdd", "atomicCounterAnd", "atomicCounterCompSwap",
    "atomicCounterDecrement", "atomicCounterExchange", "atomicCounterIncrement", "atomicCounterMax",
    "atomicCounterMin", "atomicCounterOr", "atomicCounterSubtract", "atomicCounterXor",
    "atomicExchange", "atomicExchange", "atomicMax", "atomicMax", "atomicMin", "atomicMin",
    "atomicOr", "atomicOr", "atomicXor", "atomicXor", "barrier", "barrier",
    "beginFragmentShaderOrderingINTEL", "beginInvocationInterlockARB", "beginInvocationInterlockNV",
    "binding", "bitCount", "bitCount", "bitfieldExtract", "bitfieldExtract", "bitfieldInsert",
    "bitfieldInsert", "bitfieldReverse", "bitfieldReverse", "bool", "break", "bvec2", "bvec3",
    "bvec4", "case", "ceil", "ceil", "centroid", "clamp", "clamp", "clamp", "clamp", "clamp",
    "clamp", "clamp", "clamp", "coherent", "const", "continue", "cos", "cosh", "cross", "cross",
    "dFdx", "dFdx", "dFdxCoarse", "dFdxFine", "dFdy", "dFdy", "dFdyCoarse", "dFdyFine", "default",
    "degrees", "determinant", "determinant", "determinant", "discard", "distance", "distance", "do",
    "dot", "dot", "else", "endInvocationInterlockARB", "endInvocationInterlockNV", "equal", "equal",
    "equal", "equal", "exp", "exp2", "faceforward", "faceforward", "false", "findLSB", "findLSB",
    "findMSB", "findMSB", "flat", "float", "floatBitsToInt", "floatBitsToUint", "floor", "floor",
    "fma", "fma", "fma", "for", "fract", "fract", "frexp", "frexp", "ftransform", "fwidth",
    "fwidth", "fwidthCoarse", "fwidthFine", "greaterThan", "greaterThan", "greaterThan",
    "greaterThanEqual", "greaterThanEqual", "greaterThanEqual", "groupMemoryBarrier", "highp", "if",
    "iimage2D", "image2D", "imageAtomicAdd", "imageAtomicAdd", "imageAtomicAdd", "imageAtomicAdd",
    "imageAtomicAnd", "imageAtomicAnd", "imageAtomicAnd", "imageAtomicAnd", "imageAtomicCompSwap",
    "imageAtomicCompSwap", "imageAtomicCompSwap", "imageAtomicCompSwap", "imageAtomicExchange",
    "imageAtomicExchange", "imageAtomicExchange", "imageAtomicExchange", "imageAtomicExchange",
    "imageAtomicMax", "imageAtomicMax", "imageAtomicMax", "imageAtomicMax", "imageAtomicMin",
    "imageAtomicMin", "imageAtomicMin", "imageAtomicMin", "imageAtomicOr", "imageAtomicOr",
    "imageAtomicOr", "imageAtomicOr", "imageAtomicXor", "imageAtomicXor", "imageAtomicXor",
    "imageAtomicXor", "imageLoad", "imageLoad", "imageLoad", "imageLoad", "imageLoad", "imageLoad",
    "imageLoad", "imageLoad", "imageSamples", "imageSamples", "imageSize", "imageSize", "imageSize",
    "imageSize", "imageSize", "imageSize", "imageSize", "imageSize", "imageSize", "imageSize",
    "imageSize", "imageSize", "imageSize", "imageSize", "imageSize", "imageSize", "imageSize",
    "imageSize", "imageSize", "imageStore", "imageStore", "imageStore", "imageStore", "imageStore",
    "imageStore", "imageStore", "imageStore", "imageStore", "imulExtended", "in", "inout", "int",
    "intBitsToFloat", "interpolateAtCentroid", "interpolateAtCentroid", "interpolateAtCentroid",
    "interpolateAtCentroid", "interpolateAtCentroid", "interpolateAtCentroid",
    "interpolateAtCentroid", "interpolateAtCentroid", "interpolateAtOffset", "interpolateAtOffset",
    "interpolateAtOffset", "interpolateAtOffset", "interpolateAtOffset", "interpolateAtOffset",
    "interpolateAtOffset", "interpolateAtSample", "interpolateAtSample", "interpolateAtSample",
    "interpolateAtSample", "interpolateAtSample", "interpolateAtSample", "interpolateAtSample",
    "interpolateAtSample", "invariant", "inverse", "inverse", "inverse", "inversesqrt",
    "inversesqrt", "isampler2D", "isampler2DArray", "isampler3D", "isamplerCube", "isinf", "isinf",
    "isnan", "isnan", "ivec2", "ivec3", "ivec4", "layout", "ldexp", "ldexp", "length", "length",
    "lessThan", "lessThan", "lessThan", "lessThanEqual", "lessThanEqual", "lessThanEqual",
    "location", "log", "log2", "lowp", "main", "mat2", "mat2x2", "mat2x3", "mat2x4", "mat3",
    "mat3x2", "mat3x3", "mat3x4", "mat4", "mat4x2", "mat4x3", "mat4x4", "matrixCompMult",
    "matrixCompMult", "matrixCompMult", "matrixCompMult", "matrixCompMult", "matrixCompMult",
    "matrixCompMult", "matrixCompMult", "matrixCompMult", "max", "max", "max", "max", "max", "max",
    "max", "max", "mediump", "memoryBarrier", "memoryBarrierAtomicCounter", "memoryBarrierBuffer",
    "memoryBarrierImage", "memoryBarrierShared", "min", "min", "min", "min", "min", "min", "min",
    "min", "mix", "mix", "mix", "mix", "mix", "mix", "mix", "mix", "mix", "mod", "mod", "mod",
    "mod", "modf", "modf", "noise1", "noise2", "noise3", "noise4", "noperspective", "normalize",
    "normalize", "not", "notEqual", "notEqual", "notEqual", "notEqual", "out", "outerProduct",
    "outerProduct", "outerProduct", "outerProduct", "outerProduct", "outerProduct", "outerProduct",
    "outerProduct", "outerProduct", "packDouble2x32", "packHalf2x16", "packSnorm2x16",
    "packSnorm4x8", "packUnorm2x16", "packUnorm4x8", "pixelLocalLoadANGLE", "pixelLocalStoreANGLE",
    "pow", "precision", "r16f", "r32f", "r32ui", "radians", "reflect", "reflect", "refract",
    "refract", "return", "rg16f", "rgb_2_yuv", "rgba8", "rgba8i", "rgba8ui", "round", "round",
    "roundEven", "roundEven", "sampler2D", "sampler2DArray", "sampler2DArrayShadow",
    "sampler2DShadow", "sampler3D", "samplerCube", "samplerCubeShadow", "shadow1D", "shadow1DLod",
    "shadow1DProj", "shadow1DProj", "shadow1DProjLod", "shadow2D", "shadow2D", "shadow2DEXT",
    "shadow2DLod", "shadow2DProj", "shadow2DProj", "shadow2DProjEXT", "shadow2DProjLod", "sign",
    "sign", "sign", "sin", "sinh", "smooth", "smoothstep", "smoothstep", "smoothstep", "smoothstep",
    "sqrt", "sqrt", "std140", "std430", "step", "step", "step", "step", "struct", "subpassLoad",
    "subpassLoad", "switch", "tan", "tanh", "texelFetch", "texelFetch", "texelFetch", "texelFetch",
    "texelFetch", "texelFetch", "texelFetch", "texelFetch", "texelFetch", "texelFetch",
    "texelFetch", "texelFetch", "texelFetch", "texelFetchOffset", "texelFetchOffset",
    "texelFetchOffset", "texelFetchOffset", "texelFetchOffset", "texelFetchOffset", "texture",
    "texture", "texture", "texture", "texture", "texture", "texture", "texture", "texture",
    "texture", "texture", "texture", "texture", "texture", "texture", "texture", "texture",
    "texture", "texture", "texture", "texture", "texture", "texture", "texture", "texture",
    "texture", "texture", "texture", "texture", "texture", "texture", "texture", "texture",
    "texture", "texture", "texture", "texture1D", "texture1D", "texture1DLod", "texture1DProj",
    "texture1DProj", "texture1DProj", "texture1DProj", "texture1DProjLod", "texture1DProjLod",
    "texture2D", "texture2D", "texture2D", "texture2DGradEXT", "texture2DLod", "texture2DLod",
    "texture2DLodEXT", "texture2DProj", "texture2DProj", "texture2DProj", "texture2DProj",
    "texture2DProj", "texture2DProj", "texture2DProjGradEXT", "texture2DProjGradEXT",
    "texture2DProjLod", "texture2DProjLod", "texture2DProjLod", "texture2DProjLod",
    "texture2DProjLodEXT", "texture2DProjLodEXT", "texture2DRect", "texture2DRectProj",
    "texture2DRectProj", "texture3D", "texture3D", "texture3D", "texture3D", "texture3DLod",
    "texture3DLod", "texture3DProj", "texture3DProj", "texture3DProj", "texture3DProj",
    "texture3DProjLod", "texture3DProjLod", "textureCube", "textureCube", "textureCubeGradEXT",
    "textureCubeLod", "textureCubeLod", "textureCubeLodEXT", "textureGather", "textureGather",
    "textureGather", "textureGather", "textureGather", "textureGather", "textureGather",
    "textureGather", "textureGather", "textureGather", "textureGather", "textureGather",
    "textureGather", "textureGather", "textureGather", "textureGather", "textureGather",
    "textureGather", "textureGather", "textureGather", "textureGather", "textureGatherOffset",
    "textureGatherOffset", "textureGatherOffset", "textureGatherOffset", "textureGatherOffset",
    "textureGatherOffset", "textureGatherOffset", "textureGatherOffset", "textureGatherOffset",
    "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets",
    "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets",
    "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets",
    "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets", "textureGrad",
    "textureGrad", "textureGrad", "textureGrad", "textureGrad", "textureGrad", "textureGrad",
    "textureGrad", "textureGrad", "textureGrad", "textureGrad", "textureGrad", "textureGrad",
    "textureGrad", "textureGrad", "textureGradOffset", "textureGradOffset", "textureGradOffset",
    "textureGradOffset", "textureGradOffset", "textureGradOffset", "textureGradOffset",
    "textureGradOffset", "textureGradOffset", "textureGradOffset", "textureGradOffset",
    "textureLod", "textureLod", "textureLod", "textureLod", "textureLod", "textureLod",
    "textureLod", "textureLod", "textureLod", "textureLod", "textureLod", "textureLodOffset",
    "textureLodOffset", "textureLodOffset", "textureLodOffset", "textureLodOffset",
    "textureLodOffset", "textureLodOffset", "textureLodOffset", "textureOffset", "textureOffset",
    "textureOffset", "textureOffset", "textureOffset", "textureOffset", "textureOffset",
    "textureOffset", "textureOffset", "textureOffset", "textureOffset", "textureOffset",
    "textureOffset", "textureOffset", "textureOffset", "textureOffset", "textureOffset",
    "textureOffset", "textureOffset", "textureProj", "textureProj", "textureProj", "textureProj",
    "textureProj", "textureProj", "textureProj", "textureProj", "textureProj", "textureProj",
    "textureProj", "textureProj", "textureProj", "textureProj", "textureProj", "textureProj",
    "textureProj", "textureProj", "textureProj", "textureProj", "textureProj", "textureProj",
    "textureProj", "textureProj", "textureProj", "textureProj", "textureProjGrad",
    "textureProjGrad", "textureProjGrad", "textureProjGrad", "textureProjGrad", "textureProjGrad",
    "textureProjGrad", "textureProjGrad", "textureProjGrad", "textureProjGrad",
    "textureProjGradOffset", "textureProjGradOffset", "textureProjGradOffset",
    "textureProjGradOffset", "textureProjGradOffset", "textureProjGradOffset",
    "textureProjGradOffset", "textureProjGradOffset", "textureProjGradOffset",
    "textureProjGradOffset", "textureProjLod", "textureProjLod", "textureProjLod", "textureProjLod",
    "textureProjLod", "textureProjLod", "textureProjLod", "textureProjLodOffset",
    "textureProjLodOffset", "textureProjLodOffset", "textureProjLodOffset", "textureProjLodOffset",
    "textureProjLodOffset", "textureProjLodOffset", "textureProjOffset", "textureProjOffset",
    "textureProjOffset", "textureProjOffset", "textureProjOffset", "textureProjOffset",
    "textureProjOffset", "textureProjOffset", "textureProjOffset", "textureProjOffset",
    "textureProjOffset", "textureProjOffset", "textureProjOffset", "textureProjOffset",
    "textureProjOffset", "textureProjOffset", "textureProjOffset", "textureQueryLevels",
    "textureQueryLevels", "textureQueryLevels", "textureQueryLevels", "textureQueryLevels",
    "textureQueryLevels", "textureQueryLevels", "textureQueryLevels", "textureQueryLevels",
    "textureQueryLevels", "textureQueryLevels", "textureQueryLevels", "textureQueryLevels",
    "textureQueryLod", "textureQueryLod", "textureQueryLod", "textureQueryLod", "textureQueryLod",
    "textureQueryLod", "textureQueryLod", "textureQueryLod", "textureQueryLod", "textureQueryLod",
    "textureQueryLod", "textureQueryLod", "textureQueryLod", "textureSamples", "textureSamples",
    "textureSize", "textureSize", "textureSize", "textureSize", "textureSize", "textureSize",
    "textureSize", "textureSize", "textureSize", "textureSize", "textureSize", "textureSize",
    "textureSize", "textureSize", "textureSize", "textureSize", "textureSize", "textureSize",
    "textureSize", "textureSize", "textureSize", "textureSize", "textureSize", "textureSize",
    "textureVideoWEBGL", "transpose", "transpose", "transpose", "transpose", "transpose",
    "transpose", "transpose", "transpose", "transpose", "true", "trunc", "trunc", "uaddCarry",
    "uimage2D", "uint", "uintBitsToFloat", "umulExtended", "uniform", "unpackDouble2x32",
    "unpackHalf2x16", "unpackSnorm2x16", "unpackSnorm4x8", "unpackUnorm2x16", "usampler2D",
    "usampler2DArray", "usampler3D", "usamplerCube", "usubBorrow", "uvec2", "uvec3", "uvec4",
    "vec2", "vec3", "vec4", "void", "volatile", "while", "yuv_2_rgb", "__pixel_localEXT",
    "__pixel_local_inEXT", "__pixel_local_outEXT", "set", "texture2D", "utexture2D", "sampler",
    "subpassInput", "usubpassInput", "input_attachment_index", "readonly", "buffer",
    "unpackUnorm4x8", "defined", "elif", "extension", "enable", "require", "endif", "pragma",
    "__VERSION__", "constant_id", "blend_support_all_equations",
    "blend_support_multiply", "blend_support_screen", "blend_support_overlay",
    "blend_support_darken", "blend_support_lighten", "blend_support_colordodge",
    "blend_support_colorburn", "blend_support_hardlight",
    "blend_support_softlight", "blend_support_difference",
    "blend_support_exclusion",
}

# rgba and stpq get rewritten to xyzw, so we only need to check xyzw here. This way we can keep
# renaming to names like, e.g., "rg".
xyzw_pattern = re.compile(r"^[xyzw]{1,4}$")

# HLSL registers base names can't be overwritten by macro arguments if token pasting (e.g. t##IDX).
hlsl_register_base_names = ['t', 's', 'u', 'b']

# HLSL registers (e.g., t0, u1) can't be overwritten by a #define.
hlsl_register_pattern = re.compile(r"^[tsub]\d+$")

# can we rename to or from 'name'?
def is_reserved_keyword(name):
    return name in glsl_reserved\
           or xyzw_pattern.match(name)\
           or name in hlsl_register_base_names\
           or hlsl_register_pattern.match(name)\
           or name.startswith("$")\
           or name.startswith("gl_")\
           or name.startswith("GL_")\
           or name.startswith("__pixel_local")\
           or name.endswith("ANGLE")

def remove_leading_annotation(name):
    if name[0] == '@':
        # A leading '@' indicates identifier names that should be exported. Rename '@my_var' to
        # '_EXPORTED_my_var' to enforce that '@my_var' and 'my_var' are not interchangeable.
        return '_EXPORTED_' + name[1:]
    if name[0] == '$':
        # A leading '$' indicates identifier names that should not be renamed.
        return name[1:]
    return name

# Generates new identifier names to rewrite our variables.
class NameGenerator:
    def __init__(self, first_letter_chars, additional_letter_chars):
        self.first_letter_chars = first_letter_chars
        self.additional_letter_chars = additional_letter_chars
        self.name_index = 0

    def next_name(self):
        i = self.name_index
        # Generate the first character from 'self.first_letter_chars'
        name = self.first_letter_chars[i % len(self.first_letter_chars)]
        i = i // len(self.first_letter_chars)
        while i > 0:
            # Generate the remaining characters from 'self.additional_letter_chars'
            name += self.additional_letter_chars[i % len(self.additional_letter_chars)]
            i = i // len(self.additional_letter_chars)
        self.name_index = self.name_index + 1
        return name

# Exported variables only use upper case letters in their names. HLSL semantics are not case
# sensitive and may also assign special meaning to numbers.
upper_case_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
upper_case_name_generator = NameGenerator(upper_case_chars, upper_case_chars + "_")

# Don't begin new names with the the '_' character. Internal code can begin names with '_' without
# fear of renaming collisions.
lower_and_upper_chars = "abcdefghijklmnopqrstuvwxyz" + upper_case_chars
general_name_generator = NameGenerator(lower_and_upper_chars, "_0123456789" + lower_and_upper_chars)

used_new_names = set()

def generate_new_name(*, force_upper_case):
    name_generator = upper_case_name_generator if force_upper_case else general_name_generator
    while True:
        name = name_generator.next_name()
        if not is_reserved_keyword(name) and not name in used_new_names:
            used_new_names.add(name)
            return name

# mapping from original identifiers to new names.
new_names = {}
def generate_new_names():
    for name,count in sorted(all_id_counts.items(), key=lambda x:x[1], reverse=True):
        new_name = (remove_leading_annotation(name)
                    if args.human_readable or is_reserved_keyword(name)
                    # HLSL semantics are not case sensitive and can assign special meaning to
                    # numbers. Make all exported names upper case with no numbers.
                    else generate_new_name(force_upper_case=name[0] == '@'))
        new_names[name] = new_name

# used to rewrite rgba and stpq swizzles to xyzw.
rgba_stpq_pattern = re.compile(r"^([rgba]{1,4}|[stpq]{1,4})$")
rgba_stpq_remap = {'r':'x', 'g':'y', 'b':'z', 'a':'w',
                   's':'x', 't':'y', 'p':'z', 'q':'w',}


# minifies a single GLSL file.
class Minifier:
    def __init__(self, code, basename, exports=set()):
        # parse tokens.
        lexer = lex.lex()
        lexer.exports = exports
        lexer.input(code)
        self.tokens = [tok for tok in lexer];
        self.exports = exports
        self.basename = basename


    # Strips unneeded code from the tokens. Called after all Minifiers have been parsed.
    def strip_tokens(self):
        assert(not args.human_readable)

        # strip comments.
        self.tokens = \
            [tok for tok in self.tokens if "COMMENT" not in tok.type]

        # strip unused defines.
        self.tokens = [tok for tok in self.tokens if tok.type != "DEFINE"\
                                                  or all_id_reference_counts[tok.define_id] > 0]

        # merge whitespace.
        unmerged,self.tokens = self.tokens,[]
        for tok in unmerged:
            if tok.type == "DEFINE":
                if tok.define_arglist != None:
                    tok.define_arglist.strip_tokens()
                if tok.define_val != None:
                    tok.define_val.strip_tokens()
            if tok.type == "DIRECTIVE":
                if tok.directive_val != None:
                    tok.directive_val.strip_tokens()
            if (tok.type == "WHITESPACE"
                and len(self.tokens) > 0
                and self.tokens[-1].type == "WHITESPACE"):
                self.tokens[-1].value += tok.value
            else:
                self.tokens.append(tok)


    # generates rewritten glsl from our tokens.
    def emit_tokens_to_rewritten_glsl(self, out, *, preserve_exported_switches):
        # stand-in for a null token.
        lasttoken = lambda : None
        lasttoken.type = ""
        lasttoken.value = ""

        lasttoken_needs_whitespace = False
        is_newline = True

        for tok in self.tokens:
            if tok.type == "WHITESPACE":
                if args.human_readable:
                    out.write(tok.value)
                    is_newline = tok.value[-1] == '\n'
                    lasttoken_needs_whitespace = False
                continue

            is_directive = tok.type in ["DEFINE", "IFDEF", "DIRECTIVE"]
            needs_whitespace = tok.type in ["FLOAT", "INT", "HEX", "ID", "DEFINED_ID"]
            if is_directive and not is_newline:
                out.write('\n')
            elif needs_whitespace and lasttoken_needs_whitespace:
                out.write(' ')

            # is_newline will be false once we output the token (unless this value otherwise gets
            # updated).
            is_newline = False

            if tok.type == "ID":
                if (rgba_stpq_pattern.match(tok.value)
                    and lasttoken.type == "OP"
                    and lasttoken.value == "."):
                    # convert rgba and stpq to xyzw.
                    out.write(''.join([rgba_stpq_remap[ch] for ch in tok.value]))
                else:
                    self.write_identifier(out, tok.value, preserve_exported_switches)

            elif tok.type == "DEFINE":
                out.write("#define ")
                self.write_identifier(out, tok.define_id, preserve_exported_switches)
                if tok.define_arglist != None:
                    is_newline = tok.define_arglist.emit_tokens_to_rewritten_glsl(\
                        out,\
                        preserve_exported_switches=preserve_exported_switches)
                    assert(not is_newline)
                if tok.define_val != None:
                    out.write(' ')
                    is_newline = tok.define_val.emit_tokens_to_rewritten_glsl(\
                        out,\
                        preserve_exported_switches=preserve_exported_switches)

            elif tok.type == "IFDEF":
                out.write('#')
                out.write(tok.ifdef_tag)
                out.write(' ')
                self.write_identifier(out, tok.ifdef_id, preserve_exported_switches)

            elif tok.type == "DEFINED_ID":
                out.write('defined(')
                self.write_identifier(out, tok.defined_id, preserve_exported_switches)
                out.write(')')

            elif tok.type == "DIRECTIVE":
                out.write("#")
                if tok.directive_val != None:
                    is_newline = tok.directive_val.emit_tokens_to_rewritten_glsl(\
                        out,\
                        preserve_exported_switches=preserve_exported_switches)

            else:
                out.write(tok.value)

            # Since we preserve whitespace in 'human_readable' mode, the newline after a
            # preprocessor directive will happen for us automatically unless 'human_readable' is
            # false.
            if not args.human_readable and is_directive and not is_newline:
                out.write('\n')
                is_newline = True

            lasttoken = tok
            lasttoken_needs_whitespace = needs_whitespace

        return is_newline


    def write_identifier(self, out, identifier, preserve_exported_switches):
        if preserve_exported_switches and identifier in exported_switches:
            assert(identifier[0] == '@')
            out.write(identifier[1:])
        else:
            out.write(new_names[identifier])


    def write_exports(self, outdir):
        output_path = os.path.join(outdir, os.path.splitext(self.basename)[0] + ".exports.h")
        print("Exporting %s <- %s" % (output_path, self.basename))
        out = open(output_path, "w", newline='\n')
        out.write('#pragma once\n\n')
        for exp in sorted(self.exports):
            out.write('#define GLSL_%s "%s"\n' % (exp[1:], new_names[exp]))
        out.close()


    def write_embedded_glsl(self, outdir):
        output_path = os.path.join(outdir, self.basename + ".hpp")
        print("Embedding %s <- %s" % (output_path, self.basename))
        out = open(output_path, "w", newline='\n')
        out.write("#pragma once\n\n")

        out.write('#include "%s"\n\n' % (os.path.splitext(self.basename)[0] + ".exports.h"))

        # emit shader code.
        out.write("namespace rive {\n")
        out.write("namespace gpu {\n")
        out.write("namespace glsl {\n")
        out.write('const char %s[] = R"===(' % os.path.splitext(self.basename)[0])

        is_newline = self.emit_tokens_to_rewritten_glsl(out, preserve_exported_switches=False)
        if not is_newline:
            out.write('\n')

        out.write(')===";\n')
        out.write("} // namespace glsl\n")
        out.write("} // namespace gpu\n")
        out.write("} // namespace rive")
        out.close()


    def write_offline_glsl(self, outdir):
        output_path = os.path.join(outdir, os.path.splitext(self.basename)[0] + ".minified.glsl")
        print("Minifying %s <- %s" % (output_path, self.basename))
        out = open(output_path, "w", newline='\n')
        self.emit_tokens_to_rewritten_glsl(out, preserve_exported_switches=True)
        out.close()


# parse all GLSL files before renaming. This keeps the renaming consistent across files.
minifiers = [Minifier(open(f).read(), os.path.basename(f)) for f in args.files]
generate_new_names()

# minify all GLSL files.
if not os.path.exists(args.outdir):
   os.makedirs(args.outdir)
for m in minifiers:
    if not args.human_readable:
        m.strip_tokens()
    m.write_exports(args.outdir)
    m.write_embedded_glsl(args.outdir)
    m.write_offline_glsl(args.outdir)
