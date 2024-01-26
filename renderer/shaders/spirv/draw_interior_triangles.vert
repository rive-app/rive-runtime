#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#define VERTEX
#define TARGET_VULKAN
#define OPTIONALLY_FLAT flat
#define DRAW_INTERIOR_TRIANGLES
#define ENABLE_CLIPPING
#define ENABLE_CLIP_RECT
#define ENABLE_ADVANCED_BLEND
#include "../../shaders/out/generated/glsl.minified.glsl"
#include "../../shaders/out/generated/constants.minified.glsl"
#include "../../shaders/out/generated/common.minified.glsl"
#include "../../shaders/out/generated/draw_path_common.minified.glsl"
#include "../../shaders/out/generated/draw_path.minified.glsl"
