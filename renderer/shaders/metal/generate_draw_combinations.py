import itertools
import sys

# Organizes all combinations of valid features for draw.glsl into their own custom-named namespace.
# Generates MSL code to declare each namespace and #include draw.glsl with corresponding #defines.

class Feature():
    def __init__(self, name, index):
        self.name = name
        self.index = index

# Each feature has a specific index. These must stay in sync with pls_render_context_metal_impl.mm.
DRAW_INTERIOR_TRIANGLES = Feature('DRAW_INTERIOR_TRIANGLES', 0)
ENABLE_CLIPPING = Feature('ENABLE_CLIPPING', 1)
ENABLE_CLIP_RECT =  Feature('ENABLE_CLIP_RECT', 2)
ENABLE_ADVANCED_BLEND = Feature('ENABLE_ADVANCED_BLEND', 3)
ENABLE_EVEN_ODD = Feature('ENABLE_EVEN_ODD', 4)
ENABLE_NESTED_CLIPPING = Feature('ENABLE_NESTED_CLIPPING', 5)
ENABLE_HSL_BLEND_MODES = Feature('ENABLE_HSL_BLEND_MODES', 6)

whole_program_features = {DRAW_INTERIOR_TRIANGLES,
                          ENABLE_CLIPPING,
                          ENABLE_CLIP_RECT,
                          ENABLE_ADVANCED_BLEND}

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

# Returns whether the given feature is the *simplest* set that defines a unique vertex shader.
# (Many feature sets produce identical vertex shaders.)
def is_unique_vertex_feature_set(feature_set):
    # Fragment-only features have no effect on the vertex shader.
    if fragment_only_features.intersection(feature_set):
        return False
    return True

# Organize all combinations of valid features into their own namespace.
out = open(sys.argv[1], 'w', newline='\n')
for n in range(0, len(all_features) + 1):
    for feature_set in itertools.combinations(all_features, n):
        if not is_valid_feature_set(feature_set):
            continue
        namespace_id = ['0', '0', '0', '0', '0', '0', '0']
        for feature in feature_set:
            namespace_id[feature.index] = '1'
        out.write('namespace r%s\n' % ''.join(namespace_id))
        out.write('{\n')
        if is_unique_vertex_feature_set(feature_set):
            out.write('#define VERTEX\n')
        for feature in feature_set:
            out.write('#define %s\n' % feature.name)
        out.write('#include "draw.minified.glsl"\n')
        for feature in reversed(feature_set):
            out.write('#undef %s\n' % feature.name)
        if is_unique_vertex_feature_set(feature_set):
            out.write('#undef VERTEX\n')
        out.write('}\n')
        out.write('\n')
