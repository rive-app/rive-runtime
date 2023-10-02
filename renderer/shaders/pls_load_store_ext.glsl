/*
 * Copyright 2022 Rive
 */

// The EXT_shader_pixel_local_storage extension does not provide a mechanism to load, store, or
// clear pixel local storage contents. This shader performs custom load, store, and clear
// operations via fullscreen draws.

#ifdef @VERTEX
void main()
{
    // [-1, -1] .. [+1, +1]
    gl_Position =
        vec4(mix(vec2(-1, 1), vec2(1, -1), equal(gl_VertexID & ivec2(1, 2), ivec2(0))), 0, 1);
}
#endif

#ifdef @FRAGMENT

#ifdef GL_EXT_shader_pixel_local_storage

#extension GL_EXT_shader_pixel_local_storage : require
#extension GL_ARM_shader_framebuffer_fetch : enable
#extension GL_EXT_shader_framebuffer_fetch : enable

#ifdef @CLEAR_COLOR
#if __VERSION__ > 300
layout(binding = 0, std140) uniform ClearColor { uniform highp vec4 value; }
clearColor;
#define COLOR_CLEAR_VALUE clearColor.value
#else
uniform mediump vec4 @clearColor;
#define COLOR_CLEAR_VALUE @clearColor
#endif
#endif

#ifdef @LOAD_COLOR
#ifdef GL_ARM_shader_framebuffer_fetch
#define COLOR_LOAD_VALUE gl_LastFragColorARM
#else
layout(location = 0) inout mediump vec4 fragColor;
#define COLOR_LOAD_VALUE fragColor
#endif
#endif

#ifdef @STORE_COLOR
__pixel_local_inEXT PLS
#else
__pixel_local_outEXT PLS
#endif
{
    layout(rgba8) mediump vec4 framebuffer;
    layout(r32ui) highp uint coverageCountBuffer;
    layout(rgba8) mediump vec4 originalDstColorBuffer;
    layout(r32ui) highp uint clipBuffer;
};

#ifdef @STORE_COLOR
layout(location = 0) out mediump vec4 fragColor;
#endif

void main()
{
#ifdef @CLEAR_COLOR
    framebuffer = COLOR_CLEAR_VALUE;
#endif

#ifdef @LOAD_COLOR
    framebuffer = COLOR_LOAD_VALUE;
#endif

#ifdef @CLEAR_COVERAGE
    coverageCountBuffer = 0u;
#endif

#ifdef @CLEAR_CLIP
    clipBuffer = 0u;
#endif

#ifdef @STORE_COLOR
    fragColor = framebuffer;
#endif
}

#else

// This shader is being parsed by WebGPU for introspection purposes.
layout(location = 0) out mediump vec4 unused;
void main() { unused = vec4(0, 1, 0, 1); }

#endif // GL_EXT_shader_pixel_local_storage

#endif // FRAGMENT
