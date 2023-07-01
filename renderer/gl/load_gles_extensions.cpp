/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/gles3.hpp"

#include <EGL/egl.h>

PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC glDrawArraysInstancedBaseInstanceEXT = nullptr;
PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC glDrawElementsInstancedBaseInstanceEXT = nullptr;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC
glDrawElementsInstancedBaseVertexBaseInstanceEXT = nullptr;
PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC glFramebufferFetchBarrierQCOM = nullptr;

void loadGLESExtensions(const GLExtensions& extensions)
{
    static GLExtensions loadedExtensions{};
    if (extensions.EXT_base_instance && !loadedExtensions.EXT_base_instance)
    {
        glDrawArraysInstancedBaseInstanceEXT =
            (PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC)eglGetProcAddress(
                "glDrawArraysInstancedBaseInstanceEXT");
        glDrawElementsInstancedBaseInstanceEXT =
            (PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC)eglGetProcAddress(
                "glDrawElementsInstancedBaseInstanceEXT");
        glDrawElementsInstancedBaseVertexBaseInstanceEXT =
            (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC)eglGetProcAddress(
                "glDrawElementsInstancedBaseVertexBaseInstanceEXT");
        loadedExtensions.EXT_base_instance = true;
    }
    if (extensions.QCOM_shader_framebuffer_fetch_noncoherent &&
        !loadedExtensions.QCOM_shader_framebuffer_fetch_noncoherent)
    {
        glFramebufferFetchBarrierQCOM = (PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC)eglGetProcAddress(
            "glFramebufferFetchBarrierQCOM");
        loadedExtensions.QCOM_shader_framebuffer_fetch_noncoherent = true;
    }
}
