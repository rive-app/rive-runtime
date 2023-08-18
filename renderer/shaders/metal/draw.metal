#include <metal_stdlib>

#define FRAGMENT

#define VERTEX
#include "../../../out/obj/generated/metal.minified.glsl"
#include "../../../out/obj/generated/common.minified.glsl"
#undef VERTEX

#define ENABLE_ADVANCED_BLEND
#include "../../../out/obj/generated/advanced_blend.minified.glsl"
#undef ENABLE_ADVANCED_BLEND

#define ENABLE_ADVANCED_BLEND
#define ENABLE_HSL_BLEND_MODES
#include "../../../out/obj/generated/advanced_blend.minified.glsl"
#undef ENABLE_HSL_BLEND_MODES
#undef ENABLE_ADVANCED_BLEND

#include "../../../out/obj/generated/draw_combinations.metal"
