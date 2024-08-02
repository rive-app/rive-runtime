#version 310 es
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#define VERTEX
#define TARGET_VULKAN
#define OPTIONALLY_FLAT flat
#define DRAW_IMAGE
#define DRAW_IMAGE_MESH
#include "glsl.minified.glsl"
#include "constants.minified.glsl"
#include "specialization.minified.glsl"
#include "common.minified.glsl"
#include "draw_image_mesh.minified.glsl"
