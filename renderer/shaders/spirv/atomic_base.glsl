#extension GL_EXT_samplerless_texture_functions : require
#define TARGET_VULKAN
#define ENABLE_INSTANCE_INDEX
#define PLS_IMPL_SUBPASS_LOAD
#define USING_PLS_STORAGE_TEXTURES
#define OPTIONALLY_FLAT flat
#define ENABLE_CLIPPING
#define ENABLE_CLIP_RECT
#define ENABLE_ADVANCED_BLEND
#define ENABLE_EVEN_ODD
#define ENABLE_NESTED_CLIPPING
#define ENABLE_HSL_BLEND_MODES
#include "glsl.minified.glsl"
#include "constants.minified.glsl"
#include "common.minified.glsl"
#include "advanced_blend.minified.glsl"
#include "draw_path_common.minified.glsl"
#include "atomic_draw.minified.glsl"
