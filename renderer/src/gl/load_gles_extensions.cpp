/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/gles3.hpp"

#include <EGL/egl.h>
#include <assert.h>

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

void LoadAndValidateGLESExtensions(GLCapabilities* extensions)
{
    assert(extensions != nullptr);

    static GLCapabilities loadedExtensions{};
    if (extensions->EXT_base_instance && !loadedExtensions.EXT_base_instance)
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
        if (glDrawArraysInstancedBaseInstanceEXT != nullptr &&
            glDrawElementsInstancedBaseInstanceEXT != nullptr &&
            glDrawElementsInstancedBaseVertexBaseInstanceEXT != nullptr)
        {
            loadedExtensions.EXT_base_instance = true;
        }
        else
        {
            extensions->EXT_base_instance = false;
        }
    }
    if (extensions->QCOM_shader_framebuffer_fetch_noncoherent &&
        !loadedExtensions.QCOM_shader_framebuffer_fetch_noncoherent)
    {
        glFramebufferFetchBarrierQCOM =
            (PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC)eglGetProcAddress(
                "glFramebufferFetchBarrierQCOM");
        if (glFramebufferFetchBarrierQCOM != nullptr)
        {
            loadedExtensions.QCOM_shader_framebuffer_fetch_noncoherent = true;
        }
        else
        {
            extensions->QCOM_shader_framebuffer_fetch_noncoherent = false;
        }
    }
    if (extensions->EXT_multisampled_render_to_texture &&
        !loadedExtensions.EXT_multisampled_render_to_texture)
    {
        glFramebufferTexture2DMultisampleEXT =
            (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress(
                "glFramebufferTexture2DMultisampleEXT");
        glRenderbufferStorageMultisampleEXT =
            (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress(
                "glRenderbufferStorageMultisampleEXT");
        if (glFramebufferTexture2DMultisampleEXT != nullptr &&
            glRenderbufferStorageMultisampleEXT != nullptr)
        {
            loadedExtensions.EXT_multisampled_render_to_texture = true;
        }
        else
        {
            extensions->EXT_multisampled_render_to_texture = false;
        }
    }
    if (extensions->KHR_blend_equation_advanced &&
        !loadedExtensions.KHR_blend_equation_advanced)
    {
        glBlendBarrierKHR =
            (PFNGLBLENDBARRIERKHRPROC)eglGetProcAddress("glBlendBarrierKHR");
        if (glBlendBarrierKHR != nullptr)
        {
            loadedExtensions.KHR_blend_equation_advanced = true;
        }
        else
        {
            extensions->KHR_blend_equation_advanced = false;
        }
    }
    if (extensions->EXT_shader_pixel_local_storage2 &&
        !loadedExtensions.EXT_shader_pixel_local_storage2)
    {
        glFramebufferPixelLocalStorageSizeEXT =
            (PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC)eglGetProcAddress(
                "glFramebufferPixelLocalStorageSizeEXT");
        glClearPixelLocalStorageuiEXT =
            (PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC)eglGetProcAddress(
                "glClearPixelLocalStorageuiEXT");
        if (glFramebufferPixelLocalStorageSizeEXT != nullptr &&
            glClearPixelLocalStorageuiEXT != nullptr)
        {
            loadedExtensions.EXT_shader_pixel_local_storage2 = true;
        }
        else
        {
            extensions->EXT_shader_pixel_local_storage2 = false;
        }
    }
    if (extensions->KHR_parallel_shader_compile &&
        !loadedExtensions.KHR_parallel_shader_compile)
    {
        glMaxShaderCompilerThreadsKHR =
            (PFNGLMAXSHADERCOMPILERTHREADSKHRPROC)eglGetProcAddress(
                "glMaxShaderCompilerThreadsKHR");
        if (glMaxShaderCompilerThreadsKHR != nullptr)
        {
            loadedExtensions.KHR_parallel_shader_compile = true;
        }
        else
        {
            extensions->KHR_parallel_shader_compile = false;
        }
    }
}
