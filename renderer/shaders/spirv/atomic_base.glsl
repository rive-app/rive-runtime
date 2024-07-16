#extension GL_EXT_samplerless_texture_functions : require
#define TARGET_VULKAN
#define PLS_IMPL_STORAGE_TEXTURE
#define OPTIONALLY_FLAT flat
#define ENABLE_CLIPPING
#define ENABLE_CLIP_RECT
#define ENABLE_ADVANCED_BLEND
#define ENABLE_EVEN_ODD
#define ENABLE_NESTED_CLIPPING
#define ENABLE_HSL_BLEND_MODES
#include "../../shaders/out/generated/glsl.minified.glsl"
#include "../../shaders/out/generated/constants.minified.glsl"
#include "../../shaders/out/generated/common.minified.glsl"
#include "../../shaders/out/generated/advanced_blend.minified.glsl"
#include "../../shaders/out/generated/draw_path_common.minified.glsl"
#include "../../shaders/out/generated/atomic_draw.minified.glsl"
