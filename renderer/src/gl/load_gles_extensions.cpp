/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/gles3.hpp"

#include <EGL/egl.h>

PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC
glDrawArraysInstancedBaseInstanceEXT = nullptr;
PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC
glDrawElementsInstancedBaseInstanceEXT = nullptr;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC
glDrawElementsInstancedBaseVertexBaseInstanceEXT = nullptr;
PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC glFramebufferFetchBarrierQCOM = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC
glFramebufferTexture2DMultisampleEXT = nullptr;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT =
    nullptr;
PFNGLBLENDBARRIERKHRPROC glBlendBarrierKHR = nullptr;
PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC
glFramebufferPixelLocalStorageSizeEXT = nullptr;
PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC glClearPixelLocalStorageuiEXT = nullptr;
PFNGLMAXSHADERCOMPILERTHREADSKHRPROC glMaxShaderCompilerThreadsKHR = nullptr;

void LoadGLESExtensions(const GLCapabilities& extensions)
{
    static GLCapabilities loadedExtensions{};
    if (extensions.EXT_base_instance && !loadedExtensions.EXT_base_instance)
    {
        glDrawArraysInstancedBaseInstanceEXT =
            (PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC)eglGetProcAddress(
                "glDrawArraysInstancedBaseInstanceEXT");
        glDrawElementsInstancedBaseInstanceEXT =
            (PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC)eglGetProcAddress(
                "glDrawElementsInstancedBaseInstanceEXT");
        glDrawElementsInstancedBaseVertexBaseInstanceEXT =
            (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC)
                eglGetProcAddress(
                    "glDrawElementsInstancedBaseVertexBaseInstanceEXT");
        loadedExtensions.EXT_base_instance = true;
    }
    if (extensions.QCOM_shader_framebuffer_fetch_noncoherent &&
        !loadedExtensions.QCOM_shader_framebuffer_fetch_noncoherent)
    {
        glFramebufferFetchBarrierQCOM =
            (PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC)eglGetProcAddress(
                "glFramebufferFetchBarrierQCOM");
        loadedExtensions.QCOM_shader_framebuffer_fetch_noncoherent = true;
    }
    if (extensions.EXT_multisampled_render_to_texture &&
        !loadedExtensions.EXT_multisampled_render_to_texture)
    {
        glFramebufferTexture2DMultisampleEXT =
            (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress(
                "glFramebufferTexture2DMultisampleEXT");
        glRenderbufferStorageMultisampleEXT =
            (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress(
                "glRenderbufferStorageMultisampleEXT");
        loadedExtensions.EXT_multisampled_render_to_texture = true;
    }
    if (extensions.KHR_blend_equation_advanced &&
        !loadedExtensions.KHR_blend_equation_advanced)
    {
        glBlendBarrierKHR =
            (PFNGLBLENDBARRIERKHRPROC)eglGetProcAddress("glBlendBarrierKHR");
        loadedExtensions.KHR_blend_equation_advanced = true;
    }
    if (extensions.EXT_shader_pixel_local_storage2 &&
        !loadedExtensions.EXT_shader_pixel_local_storage2)
    {
        glFramebufferPixelLocalStorageSizeEXT =
            (PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC)eglGetProcAddress(
                "glFramebufferPixelLocalStorageSizeEXT");
        glClearPixelLocalStorageuiEXT =
            (PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC)eglGetProcAddress(
                "glClearPixelLocalStorageuiEXT");
        loadedExtensions.EXT_shader_pixel_local_storage2 = true;
    }
    if (extensions.KHR_parallel_shader_compile &&
        !loadedExtensions.KHR_parallel_shader_compile)
    {
        glMaxShaderCompilerThreadsKHR =
            (PFNGLMAXSHADERCOMPILERTHREADSKHRPROC)eglGetProcAddress(
                "glMaxShaderCompilerThreadsKHR");
        loadedExtensions.KHR_parallel_shader_compile = true;
    }
}
