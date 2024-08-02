#include <metal_stdlib>

// Add baseInstance to the instanceID for path draws.
#define ENABLE_INSTANCE_INDEX

#define FRAGMENT

#define VERTEX
#include "metal.minified.glsl"
#include "constants.minified.glsl"
#define DRAW_IMAGE
#include "common.minified.glsl"
#undef DRAW_IMAGE
#define DRAW_PATH
#define DRAW_INTERIOR_TRIANGLES
#include "draw_path_common.minified.glsl"
#undef DRAW_INTERIOR_TRIANGLES
#undef DRAW_PATH
#undef VERTEX

#define ENABLE_ADVANCED_BLEND 1
#define ENABLE_HSL_BLEND_MODES 1
#include "advanced_blend.minified.glsl"
#undef ENABLE_HSL_BLEND_MODES
#undef ENABLE_ADVANCED_BLEND

#undef FRAGMENT

#include "draw_combinations.metal"
