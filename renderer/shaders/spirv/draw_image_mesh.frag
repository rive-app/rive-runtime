#version 310 es
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define TARGET_VULKAN
#define PLS_IMPL_NONE
#define OPTIONALLY_FLAT flat
#define ENABLE_CLIPPING
#define ENABLE_CLIP_RECT
#define ENABLE_ADVANCED_BLEND
#define ENABLE_HSL_BLEND_MODES
#include "../../../out/obj/generated/glsl.minified.glsl"
#include "../../../out/obj/generated/constants.minified.glsl"
#include "../../../out/obj/generated/common.minified.glsl"
#include "../../../out/obj/generated/advanced_blend.minified.glsl"
#include "../../../out/obj/generated/draw_image_mesh.minified.glsl"
