#version 310 es
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#define VERTEX
#define TARGET_VULKAN
#define ENABLE_INSTANCE_INDEX
#define OPTIONALLY_FLAT flat
#define ENABLE_CLIPPING
#define ENABLE_CLIP_RECT
#define ENABLE_ADVANCED_BLEND
#define DRAW_PATH
#include "../../shaders/out/generated/glsl.minified.glsl"
#include "../../shaders/out/generated/constants.minified.glsl"
#include "../../shaders/out/generated/common.minified.glsl"
#include "../../shaders/out/generated/draw_path_common.minified.glsl"
#include "../../shaders/out/generated/draw_path.minified.glsl"
