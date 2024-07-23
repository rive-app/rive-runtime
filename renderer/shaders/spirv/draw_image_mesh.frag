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
#define DRAW_IMAGE
#define DRAW_IMAGE_MESH
#include "glsl.minified.glsl"
#include "constants.minified.glsl"
#include "common.minified.glsl"
#include "advanced_blend.minified.glsl"
#include "draw_image_mesh.minified.glsl"
