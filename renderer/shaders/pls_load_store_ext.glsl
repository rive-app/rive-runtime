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

#ifdef @CLEAR_COLOR
uniform mediump vec4 @clearColor;
#endif

#if !defined(GL_ARM_shader_framebuffer_fetch)
#ifdef @LOAD_COLOR
layout(location = 0) inout mediump vec4 fragColor;
#endif
#endif

#ifdef @STORE_COLOR
layout(location = 0) out mediump vec4 fragColor;
#endif

void main()
{
#ifdef @CLEAR_COLOR
    framebuffer = @clearColor;
#endif

#ifdef @LOAD_COLOR
#if defined(GL_ARM_shader_framebuffer_fetch)
    framebuffer = gl_LastFragColorARM;
#else
    framebuffer = fragColor;
#endif
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

#endif // FRAGMENT
