#extension GL_EXT_samplerless_texture_functions : require
#define ENABLE_INSTANCE_INDEX
#define OPTIONALLY_FLAT flat
#define DRAW_PATH
#define ENABLE_FEATHER true
#include "glsl.minified.glsl"
#include "constants.minified.glsl"
#include "common.minified.glsl"
#include "draw_path_common.minified.glsl"
#include "render_atlas.minified.glsl"
