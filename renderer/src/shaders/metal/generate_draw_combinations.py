import itertools
import sys
from enum import Enum

# Organizes all combinations of valid features for draw.glsl into their own custom-named namespace.
# Generates MSL code to declare each namespace and #include draw.glsl with corresponding #defines.

class Feature():
    def __init__(self, name, index):
        self.name = name
        self.index = index

# Each feature has a specific index. These must stay in sync with render_context_metal_impl.mm.
ENABLE_CLIPPING = Feature('ENABLE_CLIPPING', 0)
ENABLE_CLIP_RECT =  Feature('ENABLE_CLIP_RECT', 1)
ENABLE_ADVANCED_BLEND = Feature('ENABLE_ADVANCED_BLEND', 2)
ENABLE_FEATHER = Feature('ENABLE_FEATHER', 3)
ENABLE_EVEN_ODD = Feature('ENABLE_EVEN_ODD', 4)
ENABLE_NESTED_CLIPPING = Feature('ENABLE_NESTED_CLIPPING', 5)
ENABLE_HSL_BLEND_MODES = Feature('ENABLE_HSL_BLEND_MODES', 6)
DRAW_INTERIOR_TRIANGLES = Feature('DRAW_INTERIOR_TRIANGLES', 7)
ATLAS_BLIT = Feature('ATLAS_BLIT', 8)

whole_program_features = {ENABLE_CLIPPING,
                          ENABLE_CLIP_RECT,
                          ENABLE_ADVANCED_BLEND,
                          ENABLE_FEATHER}

fragment_only_features = {ENABLE_EVEN_ODD,
                          ENABLE_NESTED_CLIPPING,
                          ENABLE_HSL_BLEND_MODES}

all_features = whole_program_features.union(fragment_only_features)

# Returns whether a valid program exists for the given feature set.
def is_valid_feature_set(feature_set):
    if ENABLE_NESTED_CLIPPING in feature_set and ENABLE_CLIPPING not in feature_set:
        return False
    if ENABLE_HSL_BLEND_MODES in feature_set and ENABLE_ADVANCED_BLEND not in feature_set:
        return False
    return True

# Returns whether the given feature set is the *simplest* set that defines a unique vertex shader.
# (Many feature sets produce identical vertex shaders.)
def is_unique_vertex_feature_set(feature_set):
    # Fragment-only features have no effect on the vertex shader.
    if fragment_only_features.intersection(feature_set):
        return False
    return True

non_atlas_coverage_features = {ENABLE_FEATHER,
                               ENABLE_EVEN_ODD,
                               ENABLE_NESTED_CLIPPING}

non_image_mesh_features = {ENABLE_FEATHER,
                           ENABLE_EVEN_ODD,
                           ENABLE_NESTED_CLIPPING,
                           DRAW_INTERIOR_TRIANGLES,
                           ATLAS_BLIT}

# Returns whether the given feature set is compatible with an image mesh shader.
def is_image_mesh_feature_set(feature_set):
    return not non_image_mesh_features.intersection(feature_set)

ShaderType = Enum('ShaderType', ['VERTEX', 'FRAGMENT'])
DrawType = Enum('DrawType', ['PATH', 'IMAGE_MESH'])
FillType = Enum('FillType', ['CLOCKWISE', 'LEGACY'])

def emit_shader(out, shader_type, draw_type, fill_type, feature_set):
    assert(is_valid_feature_set(feature_set))
    if shader_type == ShaderType.VERTEX:
        assert(is_unique_vertex_feature_set(feature_set))
        out.write('#define VERTEX\n')
    else:
        out.write('#define FRAGMENT\n')
    if draw_type == DrawType.IMAGE_MESH:
        assert(is_image_mesh_feature_set(feature_set))
    namespace_id = ['0', '0', '0', '0', '0', '0', '0', '0', '0']
    for feature in feature_set:
        namespace_id[feature.index] = '1'
    for feature in feature_set:
        out.write('#define %s 1\n' % feature.name)
    if fill_type == FillType.CLOCKWISE:
        out.write('#define CLOCKWISE_FILL 1\n')
    if draw_type == DrawType.PATH:
        out.write('#define DRAW_PATH 1\n')
        out.write('namespace %s%s\n' %
                  ('c' if fill_type == FillType.CLOCKWISE else 'p',
                   ''.join(namespace_id)))
        out.write('{\n')
        out.write('#include "draw_path.minified.vert"\n')
        if ATLAS_BLIT in feature_set:
            out.write('#include "draw_mesh.minified.frag"\n')
        else:
            out.write('#include "draw_raster_order_path.minified.frag"\n')
        out.write('}\n')
        out.write('#undef DRAW_PATH\n')
    else:
        out.write('#define DRAW_IMAGE 1\n')
        out.write('#define DRAW_IMAGE_MESH 1\n')
        out.write('namespace m%s\n' % ''.join(namespace_id))
        out.write('{\n')
        out.write('#include "draw_image_mesh.minified.vert"\n')
        out.write('#include "draw_mesh.minified.frag"\n')
        out.write('}\n')
        out.write('#undef DRAW_IMAGE_MESH\n')
        out.write('#undef DRAW_IMAGE\n')
    for feature in feature_set:
        out.write('#undef %s\n' % feature.name)
    if shader_type == ShaderType.VERTEX:
        out.write('#undef VERTEX\n')
    else:
        out.write('#undef FRAGMENT\n')
    if fill_type == FillType.CLOCKWISE:
        out.write('#undef CLOCKWISE_FILL\n')
    out.write('\n')

# Organize all combinations of valid features into their own namespace.
out = open(sys.argv[1], 'w', newline='\n')

# Precompile the bare minimum set of shaders required to draw everything. We can compile more
# specialized shaders in the background at runtime, and use the fully-featured (slower) shaders
# while waiting for the compilations to complete.

# Path tessellation shaders.
emit_shader(out, ShaderType.VERTEX, DrawType.PATH, FillType.LEGACY,
            whole_program_features)
emit_shader(out, ShaderType.FRAGMENT, DrawType.PATH, FillType.LEGACY, all_features)
emit_shader(out, ShaderType.FRAGMENT, DrawType.PATH, FillType.CLOCKWISE, all_features)

# Interior triangulation shaders.
emit_shader(out, ShaderType.VERTEX, DrawType.PATH, FillType.LEGACY,
            whole_program_features.union({DRAW_INTERIOR_TRIANGLES}))
emit_shader(out, ShaderType.FRAGMENT, DrawType.PATH, FillType.LEGACY,
            all_features.union({DRAW_INTERIOR_TRIANGLES}))
emit_shader(out, ShaderType.FRAGMENT, DrawType.PATH, FillType.CLOCKWISE,
            all_features.union({DRAW_INTERIOR_TRIANGLES}))

# Atlas blit shaders.
emit_shader(out, ShaderType.VERTEX, DrawType.PATH, FillType.LEGACY,
            whole_program_features\
                    .union({DRAW_INTERIOR_TRIANGLES, ATLAS_BLIT})\
                    .difference(non_atlas_coverage_features))
emit_shader(out, ShaderType.FRAGMENT, DrawType.PATH, FillType.LEGACY,
            all_features\
                    .union({DRAW_INTERIOR_TRIANGLES, ATLAS_BLIT})\
                    .difference(non_atlas_coverage_features))

# Image mesh shaders.
emit_shader(out, ShaderType.VERTEX, DrawType.IMAGE_MESH, FillType.LEGACY,
            whole_program_features.difference(non_image_mesh_features))
emit_shader(out, ShaderType.FRAGMENT, DrawType.IMAGE_MESH, FillType.LEGACY,
            all_features.difference(non_image_mesh_features))

# If we wanted to emit all combos...
# for n in range(0, len(all_features) + 1):
#     for feature_set in itertools.combinations(all_features, n):
#         if not is_valid_feature_set(feature_set):
#             continue
