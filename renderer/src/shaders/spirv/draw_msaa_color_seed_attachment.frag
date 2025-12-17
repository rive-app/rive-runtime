#version 460
#extension GL_GOOGLE_include_directive : require
#include "glsl.minified.glsl"
#include "constants.minified.glsl"
#include "common.minified.glsl"
#define INPUT_ATTACHMENT_BINDING 3 // MSAA_COLOR_SEED_IDX
#include "draw_input_attachment.minified.frag"
