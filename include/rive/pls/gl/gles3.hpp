/*
 * Copyright 2023 Rive
 */

#pragma once

#include <string.h>
#include <stdio.h>

struct GLExtensions
{
    bool ANGLE_base_vertex_base_instance_shader_builtin : 1;
    bool ANGLE_shader_pixel_local_storage : 1;
    bool ANGLE_shader_pixel_local_storage_coherent : 1;
    bool ANGLE_polygon_mode : 1;
    bool ANGLE_provoking_vertex : 1;
    bool ARM_shader_framebuffer_fetch : 1;
    bool ARB_bindless_texture : 1;
    bool ARB_fragment_shader_interlock : 1;
    bool EXT_base_instance : 1;
    bool INTEL_fragment_shader_ordering : 1;
    bool EXT_shader_framebuffer_fetch : 1;
    bool EXT_shader_pixel_local_storage : 1;
    bool QCOM_shader_framebuffer_fetch_noncoherent : 1;
    GLExtensions() { memset(this, 0, sizeof(*this)); }
};

#ifdef RIVE_DESKTOP_GL
#include "glad_custom.h"
#define GL_APIENTRY GLAPIENTRY
#define GL_WEBGL_shader_pixel_local_storage 1
#define GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_WEBGL GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE
#define GL_MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_WEBGL                             \
    GL_MAX_COLOR_ATTACHMENTS_WITH_ACTIVE_PIXEL_LOCAL_STORAGE_ANGLE
#define GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_WEBGL                          \
    GL_MAX_COMBINED_DRAW_BUFFERS_AND_PIXEL_LOCAL_STORAGE_PLANES_ANGLE
#define GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_WEBGL GL_PIXEL_LOCAL_STORAGE_ACTIVE_PLANES_ANGLE
#define GL_LOAD_OP_ZERO_WEBGL GL_LOAD_OP_ZERO_ANGLE
#define GL_LOAD_OP_CLEAR_WEBGL GL_LOAD_OP_CLEAR_ANGLE
#define GL_LOAD_OP_LOAD_WEBGL GL_LOAD_OP_LOAD_ANGLE
#define GL_LOAD_OP_DISABLE_WEBGL GL_LOAD_OP_DISABLE_ANGLE
#define GL_STORE_OP_STORE_WEBGL GL_STORE_OP_STORE_ANGLE
#define GL_PIXEL_LOCAL_FORMAT_WEBGL GL_PIXEL_LOCAL_FORMAT_ANGLE
#define GL_PIXEL_LOCAL_TEXTURE_NAME_WEBGL GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE
#define GL_PIXEL_LOCAL_TEXTURE_LEVEL_WEBGL GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE
#define GL_PIXEL_LOCAL_TEXTURE_LAYER_WEBGL GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE
#define GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_WEBGL GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE
#define GL_PIXEL_LOCAL_CLEAR_VALUE_INT_WEBGL GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE
#define GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_WEBGL GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE
#define glFramebufferTexturePixelLocalStorageWEBGL glFramebufferTexturePixelLocalStorageANGLE
#define glFramebufferPixelLocalClearValuefvWEBGL glFramebufferPixelLocalClearValuefvANGLE
#define glFramebufferPixelLocalClearValueivWEBGL glFramebufferPixelLocalClearValueivANGLE
#define glFramebufferPixelLocalClearValueuivWEBGL glFramebufferPixelLocalClearValueuivANGLE
#define glBeginPixelLocalStorageWEBGL glBeginPixelLocalStorageANGLE
#define glEndPixelLocalStorageWEBGL glEndPixelLocalStorageANGLE
#define glPixelLocalStorageBarrierWEBGL glPixelLocalStorageBarrierANGLE
#define GL_FIRST_VERTEX_CONVENTION_WEBGL GL_FIRST_VERTEX_CONVENTION_ANGLE
#define GL_SHADER_PIXEL_LOCAL_STORAGE_EXT 0x8F64
#endif

#ifdef RIVE_GLES
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2ext.h>

// Android doesn't load extension functions for us.
void loadGLESExtensions(const GLExtensions&);
extern PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC glDrawArraysInstancedBaseInstanceEXT;
extern PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC glDrawElementsInstancedBaseInstanceEXT;
extern PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC
    glDrawElementsInstancedBaseVertexBaseInstanceEXT;
extern PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC glFramebufferFetchBarrierQCOM;
#endif

#ifdef RIVE_IOS
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#endif

#ifdef RIVE_WEBGL
#include <GLES3/gl3.h>
#include <webgl/webgl2_ext.h>
#define GL_UNMASKED_RENDERER_WEBGL 0x9246
#define GL_FIRST_VERTEX_CONVENTION_WEBGL 0x8E4D

#endif
