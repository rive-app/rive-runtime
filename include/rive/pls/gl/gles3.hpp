/*
 * Copyright 2023 Rive
 */

#pragma once

struct GLExtensions
{
    bool ANGLE_shader_pixel_local_storage = false;
    bool ANGLE_shader_pixel_local_storage_coherent = false;
    bool ANGLE_polygon_mode = false;
    bool ANGLE_provoking_vertex = false;
    bool ARM_shader_framebuffer_fetch = false;
    bool ARB_fragment_shader_interlock = false;
    bool EXT_base_instance = false;
    bool INTEL_fragment_shader_ordering = false;
    bool EXT_shader_framebuffer_fetch = false;
    bool EXT_shader_pixel_local_storage = false;
    bool QCOM_shader_framebuffer_fetch_noncoherent = false;
};

#ifdef RIVE_DESKTOP_GL
#include "glad_custom.h"
#define GL_APIENTRY GLAPIENTRY
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
#define glPixelLocalStorageBarrierWEBGL glPixelLocalStorageBarrierANGLE
#define GL_FIRST_VERTEX_CONVENTION_WEBGL GL_FIRST_VERTEX_CONVENTION_ANGLE
#define RIVE_WEBGL
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
#endif

#ifdef RIVE_IOS
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#endif

#ifdef RIVE_WASM
#include <GLES3/gl3.h>
#include <webgl/webgl2_ext.h>
#define GL_UNMASKED_RENDERER_WEBGL 0x9246
#define GL_FIRST_VERTEX_CONVENTION_WEBGL 0x8E4D
#define RIVE_WEBGL
#endif
