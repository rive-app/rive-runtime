#version 460
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define TARGET_VULKAN
#define PLS_IMPL_SUBPASS_LOAD
#define OPTIONALLY_FLAT flat
#define DRAW_INTERIOR_TRIANGLES
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
#include "draw_path.minified.glsl"
