#version 460
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define PLS_IMPL_SUBPASS_LOAD
#define TARGET_VULKAN
#define OPTIONALLY_FLAT flat
#define DRAW_INTERIOR_TRIANGLES
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
#include "../../../out/obj/generated/draw_path.minified.glsl"
