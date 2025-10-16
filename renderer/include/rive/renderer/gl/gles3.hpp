/*
 * Copyright 2023 Rive
 */

#pragma once

#include <stdint.h>
#include <string.h>

#ifdef RIVE_DESKTOP_GL
#include "glad_custom.h"
#define GL_APIENTRY GLAPIENTRY
#define GL_SHADER_PIXEL_LOCAL_STORAGE_EXT 0x8F64
#define GL_FRAMEBUFFER_FETCH_NONCOHERENT_QCOM 0x96A2
#define glFramebufferFetchBarrierQCOM(...) RIVE_UNREACHABLE()
#define glFramebufferPixelLocalStorageSizeEXT(...) RIVE_UNREACHABLE()
#define glClearPixelLocalStorageuiEXT(...) RIVE_UNREACHABLE()
#endif

#ifdef RIVE_ANDROID
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2ext.h>
#endif

#ifdef RIVE_WEBGL
#include <GLES3/gl3.h>
#include <webgl/webgl2_ext.h>

#ifndef WEBGL_debug_renderer_info
#define WEBGL_debug_renderer_info 1
#define GL_UNMASKED_VENDOR_WEBGL 0x9245
#define GL_UNMASKED_RENDERER_WEBGL 0x9246
#endif

#ifndef GL_ANGLE_shader_pixel_local_storage
#define GL_ANGLE_shader_pixel_local_storage 1
#define GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE 0x96E0
#define GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE 0x96E1
#define GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE 0x96E2
#define GL_LOAD_OP_ZERO_ANGLE 0x96E3
#define GL_LOAD_OP_CLEAR_ANGLE 0x96E4
#define GL_LOAD_OP_LOAD_ANGLE 0x96E5
#define GL_STORE_OP_STORE_ANGLE 0x96E6
#define GL_PIXEL_LOCAL_FORMAT_ANGLE 0x96E7
#define GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE 0x96E8
#define GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE 0x96E9
#define GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE 0x96EA
#define GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE 0x96EB
#define GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE 0x96EC
#define GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE 0x96ED
extern bool webgl_enable_WEBGL_shader_pixel_local_storage_coherent();
extern bool webgl_shader_pixel_local_storage_is_coherent();
extern void glFramebufferTexturePixelLocalStorageANGLE(GLint plane,
                                                       GLuint backingtexture,
                                                       GLint level,
                                                       GLint layer);
extern void glFramebufferPixelLocalClearValuefvANGLE(GLint plane,
                                                     const GLfloat value[4]);
extern void glBeginPixelLocalStorageANGLE(GLsizei n, const GLenum loadops[]);
extern void glEndPixelLocalStorageANGLE(GLsizei n, const GLenum storeops[]);
extern void glGetFramebufferPixelLocalStorageParameterivANGLE(GLint plane,
                                                              GLenum pname,
                                                              GLint* param);
#endif

#ifndef GL_ANGLE_provoking_vertex
#define GL_ANGLE_provoking_vertex 1
#define GL_FIRST_VERTEX_CONVENTION_ANGLE 0x8E4D
#define GL_LAST_VERTEX_CONVENTION_ANGLE 0x8E4E
#define GL_PROVOKING_VERTEX_ANGLE 0x8E4F
extern bool webgl_enable_WEBGL_provoking_vertex();
extern void glProvokingVertexANGLE(GLenum provokeMode);
#endif

#ifndef GL_KHR_blend_equation_advanced
#define GL_KHR_blend_equation_advanced 1
#define GL_MULTIPLY_KHR 0x9294
#define GL_SCREEN_KHR 0x9295
#define GL_OVERLAY_KHR 0x9296
#define GL_DARKEN_KHR 0x9297
#define GL_LIGHTEN_KHR 0x9298
#define GL_COLORDODGE_KHR 0x9299
#define GL_COLORBURN_KHR 0x929A
#define GL_HARDLIGHT_KHR 0x929B
#define GL_SOFTLIGHT_KHR 0x929C
#define GL_DIFFERENCE_KHR 0x929E
#define GL_EXCLUSION_KHR 0x92A0
#define GL_HSL_HUE_KHR 0x92AD
#define GL_HSL_SATURATION_KHR 0x92AE
#define GL_HSL_COLOR_KHR 0x92AF
#define GL_HSL_LUMINOSITY_KHR 0x92B0
#define GL_BLEND_ADVANCED_COHERENT_KHR 0x9285
#define glBlendBarrierKHR(...) RIVE_UNREACHABLE()
#endif

#ifndef GL_EXT_clip_cull_distance
#define GL_EXT_clip_cull_distance 1
#define GL_CLIP_DISTANCE0_EXT 0x3000
#define GL_CLIP_DISTANCE1_EXT 0x3001
#define GL_CLIP_DISTANCE2_EXT 0x3002
#define GL_CLIP_DISTANCE3_EXT 0x3003
#endif

#ifndef GL_KHR_parallel_shader_compile
#define GL_KHR_parallel_shader_compile 1
#define GL_MAX_SHADER_COMPILER_THREADS_KHR 0x91B0
#define GL_COMPLETION_STATUS_KHR 0x91B1
#endif

#endif // RIVE_WEBGL

#if defined(RIVE_ANDROID) || defined(RIVE_WEBGL)
// GLES 3.1 functionality is pulled in as an extension. Define these to avoid
// compile errors, even if we won't use them.
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS 0x90D6
#endif

struct GLCapabilities
{
    GLCapabilities() { memset(this, 0, sizeof(*this)); }

    static bool IsVersionAtLeast(uint32_t aMajor,
                                 uint32_t aMinor,
                                 uint32_t bMajor,
                                 uint32_t bMinor)
    {
        uint64_t a = (static_cast<uint64_t>(aMajor) << 32) | aMinor;
        uint64_t b = (static_cast<uint64_t>(bMajor) << 32) | bMinor;
        return a >= b;
    }
    bool isContextVersionAtLeast(uint32_t major, uint32_t minor) const
    {
        return IsVersionAtLeast(contextVersionMajor,
                                contextVersionMinor,
                                major,
                                minor);
    }
    bool isVendorDriverVersionAtLeast(uint32_t major, uint32_t minor) const
    {
        return IsVersionAtLeast(vendorDriverVersionMajor,
                                vendorDriverVersionMinor,
                                major,
                                minor);
    }

    // Driver info.
    bool isGLES : 1;
    // Is the system OpenGL driver ANGLE? (Not WebGL via ANGLE, but the actual
    // system driver (which can also be true in a situation like
    // WebGL (probably ANGLE) -> System OpenGL ES (also ANGLE) -> Vulkan)).
    bool isANGLESystemDriver : 1;
    bool isAdreno : 1;
    bool isMali : 1;
    bool isPowerVR : 1;

    // GL version.
    uint32_t contextVersionMajor;
    uint32_t contextVersionMinor;
    uint32_t vendorDriverVersionMajor;
    uint32_t vendorDriverVersionMinor;

    // Workarounds.
    // Some Mali and PowerVR devices crash when issuing draw commands with a
    // large instancecount.
    uint32_t maxSupportedInstancesPerDrawCommand;
    // Chrome 136 crashes when trying to run Rive because it attempts to enable
    // blending on the tessellation texture, which is invalid for an integer
    // render target. The workaround is to use a floating-point tessellation
    // texture.
    // https://issues.chromium.org/issues/416294709
    bool needsFloatingPointTessellationTexture;
    // PowerVR Rogue GE8300, OpenGL ES 3.2 build 1.10@5187610 has severe pixel
    // local storage corruption issues with our renderer. Using some of the
    // EXT_shader_pixel_local_storage2 API is an apparent workaround that comes
    // with worse performance and other, less severe visual artifacts.
    bool needsPixelLocalStorage2;
    // ANGLE_shader_pixel_local_storage is currently broken with
    // GL_TEXTURE_2D_ARRAY on ANGLE's d3d11 renderer.
    bool avoidTexture2DArrayWithWebGLPLS;

    // Extensions
    bool ANGLE_base_vertex_base_instance_shader_builtin : 1;
    bool ANGLE_shader_pixel_local_storage : 1;
    bool ANGLE_shader_pixel_local_storage_coherent : 1;
    bool ANGLE_polygon_mode : 1;
    bool ANGLE_provoking_vertex : 1;
    bool ARM_shader_framebuffer_fetch : 1;
    bool ARB_fragment_shader_interlock : 1;
    bool ARB_shader_image_load_store : 1;
    bool ARB_shader_storage_buffer_object : 1;
    bool OES_shader_image_atomic : 1;
    bool KHR_blend_equation_advanced : 1;
    bool KHR_blend_equation_advanced_coherent : 1;
    bool KHR_parallel_shader_compile : 1;
    bool EXT_base_instance : 1;
    bool EXT_clip_cull_distance : 1;
    bool EXT_color_buffer_half_float : 1;
    bool EXT_color_buffer_float : 1;
    bool EXT_float_blend : 1;
    bool EXT_multisampled_render_to_texture : 1;
    bool EXT_shader_framebuffer_fetch : 1;
    bool EXT_shader_pixel_local_storage : 1;
    bool EXT_shader_pixel_local_storage2 : 1;
    bool INTEL_fragment_shader_ordering : 1;
    bool QCOM_shader_framebuffer_fetch_noncoherent : 1;
};

#ifdef RIVE_ANDROID
// EXT_base_instance.
extern PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC
    glDrawArraysInstancedBaseInstanceEXT;
extern PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC
    glDrawElementsInstancedBaseInstanceEXT;
extern PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC
    glDrawElementsInstancedBaseVertexBaseInstanceEXT;

// QCOM_shader_framebuffer_fetch_noncoherent.
extern PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC glFramebufferFetchBarrierQCOM;

// EXT_multisampled_render_to_texture.
extern PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC
    glFramebufferTexture2DMultisampleEXT;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC
    glRenderbufferStorageMultisampleEXT;

// KHR_blend_equation_advanced.
extern PFNGLBLENDBARRIERKHRPROC glBlendBarrierKHR;

// EXT_shader_pixel_local_storage2.
extern PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC
    glFramebufferPixelLocalStorageSizeEXT;
extern PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC glClearPixelLocalStorageuiEXT;

// KHR_parallel_shader_compilation
extern PFNGLMAXSHADERCOMPILERTHREADSKHRPROC glMaxShaderCompilerThreadsKHR;

// Android doesn't load extension functions for us (also, possibly some
// extensions are reported as present but the functions don't actually exist,
// this call will clear the capabilities flags for extensions that don't load,
// accordingly).
void LoadAndValidateGLESExtensions(GLCapabilities*);
#endif
