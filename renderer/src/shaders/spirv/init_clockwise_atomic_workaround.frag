#version 460
#extension GL_GOOGLE_include_directive : require
#define RENDER_MODE_CLOCKWISE_ATOMIC
// This shader doesn't need ENABLE_CLIP_RECT functionality. Disable it so that
// it does not fail to compile on systems without ClipDistance support.
#define DISABLE_CLIP_RECT_FOR_VULKAN_MSAA
#include "glsl.minified.glsl"
#include "constants.minified.glsl"
#include "common.minified.glsl"
#include "draw_path_common.minified.glsl"
#include "init_clockwise_atomic_workaround.minified.frag"
