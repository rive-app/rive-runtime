#version 310 es
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define TARGET_VULKAN
#define OPTIONALLY_FLAT flat
#include "../../shaders/out/generated/glsl.minified.glsl"
#include "../../shaders/out/generated/constants.minified.glsl"
#include "../../shaders/out/generated/common.minified.glsl"
#include "../../shaders/out/generated/tessellate.minified.glsl"
