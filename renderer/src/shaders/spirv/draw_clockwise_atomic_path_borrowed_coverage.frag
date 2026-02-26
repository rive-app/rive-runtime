#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#define ENABLE_INSTANCE_INDEX
#define OPTIONALLY_FLAT flat
#define DRAW_PATH
#define RENDER_MODE_CLOCKWISE_ATOMIC
#define NEVER_GENERATE_PREMULTIPLIED_PAINT_COLORS
#include "glsl.minified.glsl"
#include "constants.minified.glsl"
#include "specialization.minified.glsl"
#include "common.minified.glsl"
#include "draw_path_common.minified.glsl"
#include "advanced_blend.minified.glsl"
#include "draw_path.minified.vert"
#include "draw_clockwise_atomic_path_borrowed_coverage.minified.frag"
