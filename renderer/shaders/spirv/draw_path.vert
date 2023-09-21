#version 310 es
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#define VERTEX
#define TARGET_VULKAN
#define gl_VertexID gl_VertexIndex
#define gl_BaseInstance 0
#define gl_InstanceID gl_InstanceIndex
#define OPTIONALLY_FLAT flat
#define ENABLE_CLIPPING
#define ENABLE_CLIP_RECT
#define ENABLE_ADVANCED_BLEND
#include "../../../out/obj/generated/glsl.minified.glsl"
#include "../../../out/obj/generated/constants.minified.glsl"
#include "../../../out/obj/generated/common.minified.glsl"
#include "../../../out/obj/generated/draw_path.minified.glsl"
