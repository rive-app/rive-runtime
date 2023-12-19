#extension GL_EXT_samplerless_texture_functions : require
#define TARGET_VULKAN
#define PLS_IMPL_RW_TEXTURE
#define OPTIONALLY_FLAT flat
#define ENABLE_CLIPPING
#define ENABLE_CLIP_RECT
#define ENABLE_ADVANCED_BLEND
#define ENABLE_EVEN_ODD
#define ENABLE_NESTED_CLIPPING
#define ENABLE_HSL_BLEND_MODES
#include "../../../out/obj/generated/glsl.minified.glsl"
#include "../../../out/obj/generated/constants.minified.glsl"
#include "../../../out/obj/generated/common.minified.glsl"
#include "../../../out/obj/generated/advanced_blend.minified.glsl"
#include "../../../out/obj/generated/draw_path_common.minified.glsl"
#include "../../../out/obj/generated/atomic_draw.minified.glsl"
