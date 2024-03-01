#include <metal_stdlib>

#define FRAGMENT

#define VERTEX
#include "../../shaders/out/generated/metal.minified.glsl"
#include "../../shaders/out/generated/constants.minified.glsl"
#define DRAW_IMAGE
#include "../../shaders/out/generated/common.minified.glsl"
#undef DRAW_IMAGE
#define DRAW_PATH
#define DRAW_INTERIOR_TRIANGLES
#include "../../shaders/out/generated/draw_path_common.minified.glsl"
#undef DRAW_INTERIOR_TRIANGLES
#undef DRAW_PATH
#undef VERTEX

#define ENABLE_ADVANCED_BLEND
#include "../../shaders/out/generated/advanced_blend.minified.glsl"
#undef ENABLE_ADVANCED_BLEND

#define ENABLE_ADVANCED_BLEND
#define ENABLE_HSL_BLEND_MODES
#include "../../shaders/out/generated/advanced_blend.minified.glsl"
#undef ENABLE_HSL_BLEND_MODES
#undef ENABLE_ADVANCED_BLEND

#undef FRAGMENT

#include "../../shaders/out/generated/draw_combinations.metal"
