#version 310 es
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define TARGET_VULKAN
#define OPTIONALLY_FLAT flat
#include "glsl.minified.glsl"
#include "constants.minified.glsl"
#include "common.minified.glsl"
#include "tessellate.minified.glsl"
