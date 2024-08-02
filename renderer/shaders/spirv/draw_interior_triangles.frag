#version 460
#extension GL_GOOGLE_include_directive : require
#define FRAGMENT
#define TARGET_VULKAN
#define PLS_IMPL_SUBPASS_LOAD
#define OPTIONALLY_FLAT flat
#define DRAW_INTERIOR_TRIANGLES
#include "glsl.minified.glsl"
#include "constants.minified.glsl"
#include "specialization.minified.glsl"
#include "common.minified.glsl"
#include "advanced_blend.minified.glsl"
#include "draw_path.minified.glsl"
