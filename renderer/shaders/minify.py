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
        - No new name includes the '_' character, so internal code can use '_' without fear of
          renaming collisions.
        - GLSL keywords and builtins are not renamed.

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
    "DIRECTIVE",
    "LINE_COMMENT",
    "BLOCK_COMMENT",
    "WHITESPACE",
    "ANNOTATION",
    "OP",
    "FLOAT",
    "HEX",
    "INT",
    "ID",
]

# tracks which identifiers are declared via a #define macro
defines = set()

# counts the number of times each ID is seen, to prioritize which IDs get the shortest names
all_id_counts = defaultdict(int);
def parse_id(name, exports):
    all_id_counts[name] += 1
    # identifiers that begin with '@' get exported to C++ through #defines.
    if name[0] == '@':
        exports.add(name)

# lexing functions used by PLY.
def t_DEFINE(tok):
    r"\#[ \t]*define[ \t]+(?P<id>\@?[A-Za-z_][A-Za-z0-9_]*)(?P<val>(((\\\n|.)(?!\/[\/\*]))*))"
    tok.define_id = re.match(t_DEFINE.__doc__, tok.value)['id']
    tok.define_val = re.match(t_DEFINE.__doc__, tok.value)['val']
    defines.add(tok.define_id)
    parse_id(tok.define_id, tok.lexer.exports)
    tok.lexer.lineno += tok.value.count('\n')
    return tok

def t_IFDEF(tok):
    r"(?P<tag>\#[ \t]*ifn?def)[ \t]+(?P<id>\@?[A-Za-z_][A-Za-z0-9_]*)"
    tok.ifdef_tag = re.match(t_IFDEF.__doc__, tok.value)['tag']
    tok.ifdef_id = re.match(t_IFDEF.__doc__, tok.value)['id']
    parse_id(tok.ifdef_id, tok.lexer.exports)
    return tok

def t_DIRECTIVE(tok):
    r"\#[ \t]*(?P<val>(((\\\n|.)(?!\/[\/\*]))*))"
    tok.directive_val = re.match(t_DIRECTIVE.__doc__, tok.value)['val']
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
    r"\s+"
    tok.lexer.lineno += tok.value.count('\n')
    return tok

def t_ANNOTATION(tok):
    r"\[\[.*\]\]"
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
    r"\@?[A-Za-z_][A-Za-z0-9_]*"
    parse_id(tok.value, tok.lexer.exports)
    return tok

def t_error(tok):
    raise Exception("Illegal character '%s' at line %d" % (tok.value[0], tok.lexer.lineno))

# identifier names that cannot be renamed
glsl_reserved = {
    "const", "uniform", "layout", "location", "centroid", "flat", "smooth", "break", "continue",
    "do", "for", "while", "switch", "case", "default", "if", "else", "in", "out", "inout", "float",
    "int", "void", "bool", "true", "false", "invariant", "discard", "return", "mat2", "mat3",
    "mat4", "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3", "mat3x4", "mat4x2", "mat4x3",
    "mat4x4", "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4", "uint",
    "uvec2", "uvec3", "uvec4", "lowp", "mediump", "highp", "precision", "sampler2D", "sampler3D",
    "samplerCube", "sampler2DShadow", "samplerCubeShadow", "sampler2DArray", "sampler2DArrayShadow",
    "isampler2D", "isampler3D", "isamplerCube", "isampler2DArray", "usampler2D", "usampler3D",
    "usamplerCube", "usampler2DArray", "struct", "radians", "degrees", "sin", "cos", "tan", "asin",
    "acos", "atan", "atan", "sinh", "cosh", "tanh", "asinh", "acosh", "atanh", "pow", "exp", "log",
    "exp2", "log2", "sqrt", "sqrt", "inversesqrt", "inversesqrt", "abs", "abs", "abs", "sign",
    "sign", "sign", "floor", "floor", "trunc", "trunc", "round", "round", "roundEven", "roundEven",
    "ceil", "ceil", "fract", "fract", "mod", "mod", "mod", "mod", "min", "min", "min", "min", "min",
    "min", "min", "min", "max", "max", "max", "max", "max", "max", "max", "max", "clamp", "clamp",
    "clamp", "clamp", "clamp", "clamp", "clamp", "clamp", "mix", "mix", "mix", "mix", "mix", "mix",
    "mix", "mix", "mix", "step", "step", "step", "step", "smoothstep", "smoothstep", "smoothstep",
    "smoothstep", "modf", "modf", "isnan", "isnan", "isinf", "isinf", "floatBitsToInt",
    "floatBitsToUint", "intBitsToFloat", "uintBitsToFloat", "fma", "fma", "fma", "frexp", "frexp",
    "ldexp", "ldexp", "packSnorm2x16", "packHalf2x16", "unpackSnorm2x16", "unpackHalf2x16",
    "packUnorm2x16", "unpackUnorm2x16", "packUnorm4x8", "packSnorm4x8", "unpackSnorm4x8",
    "packDouble2x32", "unpackDouble2x32", "length", "length", "distance", "distance", "dot", "dot",
    "cross", "cross", "normalize", "normalize", "faceforward", "faceforward", "reflect", "reflect",
    "refract", "refract", "ftransform", "matrixCompMult", "matrixCompMult", "matrixCompMult",
    "matrixCompMult", "matrixCompMult", "matrixCompMult", "matrixCompMult", "matrixCompMult",
    "matrixCompMult", "outerProduct", "outerProduct", "outerProduct", "outerProduct",
    "outerProduct", "outerProduct", "outerProduct", "outerProduct", "outerProduct", "transpose",
    "transpose", "transpose", "transpose", "transpose", "transpose", "transpose", "transpose",
    "transpose", "determinant", "determinant", "determinant", "inverse", "inverse", "inverse",
    "lessThan", "lessThan", "lessThan", "lessThanEqual", "lessThanEqual", "lessThanEqual",
    "greaterThan", "greaterThan", "greaterThan", "greaterThanEqual", "greaterThanEqual",
    "greaterThanEqual", "equal", "equal", "equal", "equal", "notEqual", "notEqual", "notEqual",
    "notEqual", "any", "all", "not", "bitfieldExtract", "bitfieldExtract", "bitfieldInsert",
    "bitfieldInsert", "bitfieldReverse", "bitfieldReverse", "bitCount", "bitCount", "findLSB",
    "findLSB", "findMSB", "findMSB", "uaddCarry", "usubBorrow", "umulExtended", "imulExtended",
    "texture2D", "texture2DProj", "texture2DProj", "textureCube", "texture1D", "texture1DProj",
    "texture1DProj", "texture3D", "texture3DProj", "shadow1D", "shadow1DProj", "shadow2D",
    "shadow2DProj", "texture3D", "texture3DProj", "shadow2DEXT", "shadow2DProjEXT", "texture2D",
    "texture2DProj", "texture2DProj", "texture2DRect", "texture2DRectProj", "texture2DRectProj",
    "texture2DGradEXT", "texture2DProjGradEXT", "texture2DProjGradEXT", "textureCubeGradEXT",
    "textureVideoWEBGL", "texture2D", "texture2DProj", "texture2DProj", "textureCube", "texture3D",
    "texture3DProj", "texture1D", "texture1DProj", "texture1DProj", "shadow1DProj", "shadow2D",
    "shadow2DProj", "texture3D", "texture3DProj", "texture2DLod", "texture2DProjLod",
    "texture2DProjLod", "textureCubeLod", "texture1DLod", "texture1DProjLod", "texture1DProjLod",
    "shadow1DLod", "shadow1DProjLod", "shadow2DLod", "shadow2DProjLod", "texture3DLod",
    "texture3DProjLod", "texture3DLod", "texture3DProjLod", "texture2DLod", "texture2DProjLod",
    "texture2DProjLod", "textureCubeLod", "texture2DLodEXT", "texture2DProjLodEXT",
    "texture2DProjLodEXT", "textureCubeLodEXT", "texture", "texture", "texture", "texture",
    "texture", "texture", "texture", "texture", "texture", "texture", "texture", "texture",
    "texture", "texture", "texture", "texture", "texture", "texture", "texture", "texture",
    "texture", "textureProj", "textureProj", "textureProj", "textureProj", "textureProj",
    "textureProj", "textureProj", "textureProj", "textureProj", "textureProj", "textureProj",
    "textureProj", "textureProj", "textureProj", "textureProj", "textureLod", "textureLod",
    "textureLod", "textureLod", "textureLod", "textureLod", "textureLod", "textureLod",
    "textureLod", "textureLod", "textureLod", "textureSize", "textureSize", "textureSize",
    "textureSize", "textureSize", "textureSize", "textureSize", "textureSize", "textureSize",
    "textureSize", "textureSize", "textureSize", "textureSize", "textureSize", "textureSize",
    "textureSize", "textureSize", "textureSize", "textureSize", "textureSize", "textureSize",
    "textureSize", "textureSize", "textureSize", "textureProjLod", "textureProjLod",
    "textureProjLod", "textureProjLod", "textureProjLod", "textureProjLod", "textureProjLod",
    "texelFetch", "texelFetch", "texelFetch", "texelFetch", "texelFetch", "texelFetch",
    "texelFetch", "texelFetch", "texelFetch", "texelFetch", "texelFetch", "texelFetch",
    "texelFetch", "textureGrad", "textureGrad", "textureGrad", "textureGrad", "textureGrad",
    "textureGrad", "textureGrad", "textureGrad", "textureGrad", "textureGrad", "textureGrad",
    "textureGrad", "textureGrad", "textureGrad", "textureGrad", "textureProjGrad",
    "textureProjGrad", "textureProjGrad", "textureProjGrad", "textureProjGrad", "textureProjGrad",
    "textureProjGrad", "textureProjGrad", "textureProjGrad", "textureProjGrad",
    "textureQueryLevels", "textureQueryLevels", "textureQueryLevels", "textureQueryLevels",
    "textureQueryLevels", "textureQueryLevels", "textureQueryLevels", "textureQueryLevels",
    "textureQueryLevels", "textureQueryLevels", "textureQueryLevels", "textureQueryLevels",
    "textureQueryLevels", "textureSamples", "textureSamples", "texture", "texture", "texture",
    "texture", "textureProj", "textureProj", "textureProj", "texture", "texture", "textureProj",
    "texture", "texture", "texture", "texture", "texture", "textureProj", "textureProj",
    "textureProj", "texture", "texture", "texture", "textureProj", "textureProj", "texture",
    "textureProj", "textureProj", "textureQueryLod", "textureQueryLod", "textureQueryLod",
    "textureQueryLod", "textureQueryLod", "textureQueryLod", "textureQueryLod", "textureQueryLod",
    "textureQueryLod", "textureQueryLod", "textureQueryLod", "textureQueryLod", "textureQueryLod",
    "textureOffset", "textureOffset", "textureOffset", "textureOffset", "textureOffset",
    "textureOffset", "textureOffset", "textureOffset", "textureOffset", "textureOffset",
    "textureOffset", "textureProjOffset", "textureProjOffset", "textureProjOffset",
    "textureProjOffset", "textureProjOffset", "textureProjOffset", "textureProjOffset",
    "textureProjOffset", "textureProjOffset", "textureProjOffset", "textureLodOffset",
    "textureLodOffset", "textureLodOffset", "textureLodOffset", "textureLodOffset",
    "textureLodOffset", "textureLodOffset", "textureLodOffset", "textureProjLodOffset",
    "textureProjLodOffset", "textureProjLodOffset", "textureProjLodOffset", "textureProjLodOffset",
    "textureProjLodOffset", "textureProjLodOffset", "texelFetchOffset", "texelFetchOffset",
    "texelFetchOffset", "texelFetchOffset", "texelFetchOffset", "texelFetchOffset",
    "textureGradOffset", "textureGradOffset", "textureGradOffset", "textureGradOffset",
    "textureGradOffset", "textureGradOffset", "textureGradOffset", "textureGradOffset",
    "textureGradOffset", "textureGradOffset", "textureGradOffset", "textureProjGradOffset",
    "textureProjGradOffset", "textureProjGradOffset", "textureProjGradOffset",
    "textureProjGradOffset", "textureProjGradOffset", "textureProjGradOffset",
    "textureProjGradOffset", "textureProjGradOffset", "textureProjGradOffset", "textureOffset",
    "textureOffset", "textureOffset", "textureOffset", "textureOffset", "textureOffset",
    "textureOffset", "textureOffset", "textureProjOffset", "textureProjOffset", "textureProjOffset",
    "textureProjOffset", "textureProjOffset", "textureProjOffset", "textureProjOffset",
    "textureGather", "textureGather", "textureGather", "textureGather", "textureGather",
    "textureGather", "textureGather", "textureGather", "textureGather", "textureGather",
    "textureGather", "textureGather", "textureGather", "textureGather", "textureGather",
    "textureGather", "textureGather", "textureGather", "textureGather", "textureGather",
    "textureGather", "textureGatherOffset", "textureGatherOffset", "textureGatherOffset",
    "textureGatherOffset", "textureGatherOffset", "textureGatherOffset", "textureGatherOffset",
    "textureGatherOffset", "textureGatherOffset", "textureGatherOffsets", "textureGatherOffsets",
    "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets",
    "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets",
    "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets", "textureGatherOffsets",
    "textureGatherOffsets", "rgb_2_yuv", "yuv_2_rgb", "dFdx", "dFdy", "fwidth", "dFdx", "dFdy",
    "fwidth", "dFdxFine", "dFdyFine", "dFdxCoarse", "dFdyCoarse", "fwidthFine", "fwidthCoarse",
    "interpolateAtCentroid", "interpolateAtCentroid", "interpolateAtCentroid",
    "interpolateAtCentroid", "interpolateAtSample", "interpolateAtSample", "interpolateAtSample",
    "interpolateAtSample", "interpolateAtOffset", "interpolateAtOffset", "interpolateAtOffset",
    "interpolateAtOffset", "interpolateAtCentroid", "interpolateAtCentroid",
    "interpolateAtCentroid", "interpolateAtCentroid", "interpolateAtSample", "interpolateAtSample",
    "interpolateAtSample", "interpolateAtSample", "interpolateAtOffset", "interpolateAtOffset",
    "interpolateAtOffset", "atomicCounter", "atomicCounterIncrement", "atomicCounterDecrement",
    "atomicCounterAdd", "atomicCounterSubtract", "atomicCounterMin", "atomicCounterMax",
    "atomicCounterAnd", "atomicCounterOr", "atomicCounterXor", "atomicCounterExchange",
    "atomicCounterCompSwap", "atomicAdd", "atomicAdd", "atomicMin", "atomicMin", "atomicMax",
    "atomicMax", "atomicAnd", "atomicAnd", "atomicOr", "atomicOr", "atomicXor", "atomicXor",
    "atomicExchange", "atomicExchange", "atomicCompSwap", "atomicCompSwap", "imageStore",
    "imageStore", "imageStore", "imageStore", "imageStore", "imageStore", "imageStore",
    "imageStore", "imageStore", "imageLoad", "imageLoad", "imageLoad", "imageLoad", "imageLoad",
    "imageLoad", "imageLoad", "imageLoad", "imageSize", "imageSize", "imageSize", "imageSize",
    "imageSize", "imageSize", "imageSize", "imageSize", "imageSize", "imageSize", "imageSize",
    "imageSize", "imageSize", "imageSize", "imageSize", "imageSize", "imageSize", "imageSize",
    "imageSize", "imageSamples", "imageSamples", "imageAtomicAdd", "imageAtomicAdd",
    "imageAtomicMin", "imageAtomicMin", "imageAtomicMax", "imageAtomicMax", "imageAtomicAnd",
    "imageAtomicAnd", "imageAtomicOr", "imageAtomicOr", "imageAtomicXor", "imageAtomicXor",
    "imageAtomicExchange", "imageAtomicExchange", "imageAtomicExchange", "imageAtomicCompSwap",
    "imageAtomicCompSwap", "imageAtomicAdd", "imageAtomicAdd", "imageAtomicMin", "imageAtomicMin",
    "imageAtomicMax", "imageAtomicMax", "imageAtomicAnd", "imageAtomicAnd", "imageAtomicOr",
    "imageAtomicOr", "imageAtomicXor", "imageAtomicXor", "imageAtomicExchange",
    "imageAtomicExchange", "imageAtomicCompSwap", "imageAtomicCompSwap", "pixelLocalLoadANGLE",
    "pixelLocalStoreANGLE", "beginInvocationInterlockNV", "endInvocationInterlockNV",
    "beginFragmentShaderOrderingINTEL", "beginInvocationInterlockARB", "endInvocationInterlockARB",
    "noise1", "noise2", "noise3", "noise4", "memoryBarrier", "memoryBarrierAtomicCounter",
    "memoryBarrierBuffer", "memoryBarrierImage", "barrier", "memoryBarrierShared",
    "groupMemoryBarrier", "barrier", "EmitVertex", "EndPrimitive", "EmitVertex", "EndPrimitive",
    "EmitStreamVertex", "EndStreamPrimitive", "subpassLoad", "subpassLoad", "anyInvocation",
    "allInvocations", "allInvocationsEqual", "r32ui", "rg16f", "rgba8", "rgba8ui", "rgba8i", "r32f",
    "main",

    # keywords borrowed from metal, used by metal.glsl
    "using", "namespace", "metal", "inline", "template", "vec", "as_type", "constant",
}

# rgba and stpq get rewritten to xyzw, so we only need to check xyzw here. This way we can keep
# renaming to names like, e.g., "rg".
xyzw_pattern = re.compile(r"^[xyzw]{1,4}$")

# can we rename to or from 'name'?
def is_reserved_keyword(name):
    return name in glsl_reserved\
           or xyzw_pattern.match(name)\
           or name.startswith("gl_")\
           or name.startswith("__pixel_local")\
           or name.endswith("ANGLE")

# A leading '@' indicates identifier names that should be exported. This method removes it, if it
# exists.
def remove_leading_at(name):
    return name[1:] if name[0] == '@' else name

# generates new identifier names to rewrite our variables.
# exclude '_' from our new names. Internal variables can use '_' to avoid naming collisions.
new_name_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
next_new_name_index = 0
def generate_new_name():
    global next_new_name_index, new_name_chars
    while True:
        i = next_new_name_index
        next_new_name_index = next_new_name_index + 1
        name = new_name_chars[0] if i == 0 else ""
        while i > 0:
            name += new_name_chars[i % len(new_name_chars)]
            i = i // len(new_name_chars)
        if not is_reserved_keyword(name):
            return name

# mapping from original identifiers to new names.
new_names = {}
def generate_new_names():
    for name,count in sorted(all_id_counts.items(), key=lambda x:x[1], reverse=True):
        new_name = (remove_leading_at(name)
                    if args.human_readable or is_reserved_keyword(name)
                    else generate_new_name())
        new_names[name] = new_name

# used to rewrite rgba and stpq swizzles to xyzw.
rgba_stpq_pattern = re.compile(r"^([rgba]{1,4}|[stpq]{1,4})$")
rgba_stpq_remap = {'r':'x', 'g':'y', 'b':'z', 'a':'w',
                   's':'x', 't':'y', 'p':'z', 'q':'w',}


# minifies a single GLSL file.
class Minifier:
    def __init__(self, in_filename):
        # parse tokens.
        lexer = lex.lex()
        lexer.exports = set()
        lexer.input(open(in_filename).read())
        self.tokens = [tok for tok in lexer];
        self.exports = lexer.exports
        self.basename = os.path.basename(in_filename)


    # Strips unneeded code from the tokens. Called after all Minifiers have been parsed.
    def strip_tokens(self):
        assert(not args.human_readable)

        # strip comments.
        self.tokens = \
            [t for t in self.tokens if "COMMENT" not in t.type]

        # strip unused defines.
        self.tokens = \
            [t for t in self.tokens if t.type != "DEFINE" or all_id_counts[t.define_id] > 1]

        # merge whitespace.
        unmerged,self.tokens = self.tokens,[]
        for t in unmerged:
            if t.type == "WHITESPACE" and len(self.tokens) > 0 and self.tokens[-1].type == "WHITESPACE":
                self.tokens[-1].value += t.value
            else:
                self.tokens.append(t)


    # generates rewritten glsl from our tokens.
    def emit_tokens_to_rewritten_glsl(self, out, *, rename_exported_defines):
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
            needs_whitespace = tok.type in ["FLOAT", "INT", "HEX", "ID"]
            if is_directive and not is_newline:
                out.write('\n')
            elif needs_whitespace and lasttoken_needs_whitespace:
                out.write(' ')

            if tok.type == "ID":
                if (rgba_stpq_pattern.match(tok.value)
                    and lasttoken.type == "OP"
                    and lasttoken.value == "."):
                    # convert rgba and stpq to xyzw.
                    out.write(''.join([rgba_stpq_remap[ch] for ch in tok.value]))
                elif not rename_exported_defines and tok.value in defines and tok.value[0] == '@':
                    out.write(tok.value[1:])
                else:
                    out.write(new_names[tok.value])

            elif tok.type == "DEFINE":
                out.write("#define ")
                out.write(new_names[tok.define_id]
                          if rename_exported_defines or tok.define_id[0] != '@'
                          else tok.define_id[1:])
                if tok.define_val != None:
                    out.write(tok.define_val)

            elif tok.type == "IFDEF":
                out.write(tok.ifdef_tag)
                out.write(' ')
                out.write(new_names[tok.ifdef_id]
                          if rename_exported_defines or tok.ifdef_id[0] != '@'
                          else tok.ifdef_id[1:])

            elif tok.type == "DIRECTIVE":
                out.write("#")
                if tok.directive_val != None:
                    out.write(tok.directive_val)

            else:
                out.write(tok.value)

            if is_directive and not args.human_readable:
                out.write('\n')
                is_newline = True
            else:
                # Since we preserve whitespace in 'human_readable' mode, the newline after a
                # preprocessor directive will happen automatically.
                is_newline = False

            lasttoken = tok
            lasttoken_needs_whitespace = needs_whitespace

        if not is_newline:
            out.write('\n')


    def write_exports(self, outdir):
        output_path = os.path.join(outdir, os.path.splitext(self.basename)[0] + ".exports.h")
        print("Exporting %s identifiers -> %s" % (self.basename, output_path))
        out = open(output_path, "w", newline='\n')
        out.write('#pragma once\n\n')
        for exp in sorted(self.exports):
            out.write('#define GLSL_%s "%s"\n' % (exp[1:], new_names[exp]))
        out.close()


    def write_embedded_glsl(self, outdir):
        output_path = os.path.join(outdir, self.basename + ".hpp")
        print("Embedding %s -> %s" % (self.basename, output_path))
        out = open(output_path, "w", newline='\n')
        out.write("#pragma once\n\n")

        out.write('#include "%s"\n\n' % (os.path.splitext(self.basename)[0] + ".exports.h"))

        # emit shader code.
        out.write("namespace rive {\n")
        out.write("namespace pls {\n")
        out.write("namespace glsl {\n")
        out.write('const char %s[] = R"===(' % os.path.splitext(self.basename)[0])

        self.emit_tokens_to_rewritten_glsl(out, rename_exported_defines=True)

        out.write(')===";\n')
        out.write("} // namespace glsl\n")
        out.write("} // namespace pls\n")
        out.write("} // namespace rive")
        out.close()


    def write_offline_glsl(self, outdir):
        output_path = os.path.join(outdir, os.path.splitext(self.basename)[0] + ".minified.glsl")
        print("Minifying %s -> %s" % (self.basename, output_path))
        out = open(output_path, "w", newline='\n')
        self.emit_tokens_to_rewritten_glsl(out, rename_exported_defines=False)
        out.close()


# parse all GLSL files before renaming. This keeps the renaming consistent across files.
minifiers = [Minifier(f) for f in args.files]
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
