#version 310 es
#extension GL_GOOGLE_include_directive : require
#define VERTEX
#define TARGET_VULKAN
#define gl_VertexID gl_VertexIndex
#define gl_BaseInstance 0
#define gl_InstanceID gl_InstanceIndex
#include "../../../out/obj/generated/glsl.minified.glsl"
#include "../../../out/obj/generated/constants.minified.glsl"
#include "../../../out/obj/generated/common.minified.glsl"
#include "../../../out/obj/generated/color_ramp.minified.glsl"
