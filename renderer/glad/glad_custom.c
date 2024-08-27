#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad_custom.h"

int GLAD_GL_version_es = 1;
int GLAD_GL_version_major = 1;
int GLAD_GL_version_minor = 0;

static void load_Desktop_GL(GLADloadproc load) {
    if (GLAD_GL_version_es) return;

    GLAD_GL_ANGLE_polygon_mode = 1;
    glad_glPolygonModeANGLE = (PFNGLPOLYGONMODEANGLEPROC)load("glPolygonMode");

    if (GLAD_IS_GL_VERSION_AT_LEAST(4, 2)) {
        GLAD_GL_EXT_base_instance = 1;
        glad_glDrawArraysInstancedBaseInstanceEXT = (PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC)load("glDrawArraysInstancedBaseInstance");
        glad_glDrawElementsInstancedBaseInstanceEXT = (PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC)load("glDrawElementsInstancedBaseInstance");
        glad_glDrawElementsInstancedBaseVertexBaseInstanceEXT = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC)load("glDrawElementsInstancedBaseVertexBaseInstance");
    }

    if (GLAD_IS_GL_VERSION_AT_LEAST(4, 6))
    {
        GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin = 1;
    }
}

PFNGLFRAMEBUFFERMEMORYLESSPIXELLOCALSTORAGEANGLEPROC glad_glFramebufferMemorylessPixelLocalStorageANGLE = NULL;
PFNGLFRAMEBUFFERTEXTUREPIXELLOCALSTORAGEANGLEPROC glad_glFramebufferTexturePixelLocalStorageANGLE = NULL;
PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEFVANGLEPROC glad_glFramebufferPixelLocalClearValuefvANGLE = NULL;
PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEIVANGLEPROC glad_glFramebufferPixelLocalClearValueivANGLE = NULL;
PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEUIVANGLEPROC glad_glFramebufferPixelLocalClearValueuivANGLE = NULL;
PFNGLBEGINPIXELLOCALSTORAGEANGLEPROC glad_glBeginPixelLocalStorageANGLE = NULL;
PFNGLENDPIXELLOCALSTORAGEANGLEPROC glad_glEndPixelLocalStorageANGLE = NULL;
PFNGLPIXELLOCALSTORAGEBARRIERANGLEPROC glad_glPixelLocalStorageBarrierANGLE = NULL;
PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERFVANGLEPROC glad_glGetFramebufferPixelLocalStorageParameterfvANGLE = NULL;
PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERIVANGLEPROC glad_glGetFramebufferPixelLocalStorageParameterivANGLE = NULL;
PFNGLPOLYGONMODEANGLEPROC glad_glPolygonModeANGLE = NULL;
PFNGLPROVOKINGVERTEXANGLEPROC glad_glProvokingVertexANGLE = NULL;
PFNGLGETTEXTUREHANDLEARB glad_glGetTextureHandleARB = NULL;
PFNGLMAKETEXTUREHANDLERESIDENTARB glad_glMakeTextureHandleResidentARB = NULL;
PFNGLMAKETEXTUREHANDLENONRESIDENTARB glad_glMakeTextureHandleNonResidentARB = NULL;
/* #ifdef RIVE_DESKTOP_GL */
/* #endif */
int GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin = 0;
int GLAD_GL_ANGLE_shader_pixel_local_storage = 0;
int GLAD_GL_ANGLE_shader_pixel_local_storage_coherent = 0;
int GLAD_GL_ANGLE_polygon_mode = 0;
int GLAD_GL_ANGLE_provoking_vertex = 0;
int GLAD_GL_ARB_bindless_texture = 0;
static void load_GL_ANGLE_shader_pixel_local_storage(GLADloadproc load) {
    if(!GLAD_GL_ANGLE_shader_pixel_local_storage) return;
    glad_glFramebufferMemorylessPixelLocalStorageANGLE = (PFNGLFRAMEBUFFERMEMORYLESSPIXELLOCALSTORAGEANGLEPROC)load("glFramebufferMemorylessPixelLocalStorageANGLE");
    glad_glFramebufferTexturePixelLocalStorageANGLE = (PFNGLFRAMEBUFFERTEXTUREPIXELLOCALSTORAGEANGLEPROC)load("glFramebufferTexturePixelLocalStorageANGLE");
    glad_glFramebufferPixelLocalClearValuefvANGLE = (PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEFVANGLEPROC)load("glFramebufferPixelLocalClearValuefvANGLE");
    glad_glFramebufferPixelLocalClearValueivANGLE = (PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEIVANGLEPROC)load("glFramebufferPixelLocalClearValueivANGLE");
    glad_glFramebufferPixelLocalClearValueuivANGLE = (PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEUIVANGLEPROC)load("glFramebufferPixelLocalClearValueuivANGLE");
    glad_glBeginPixelLocalStorageANGLE = (PFNGLBEGINPIXELLOCALSTORAGEANGLEPROC)load("glBeginPixelLocalStorageANGLE");
    glad_glEndPixelLocalStorageANGLE = (PFNGLENDPIXELLOCALSTORAGEANGLEPROC)load("glEndPixelLocalStorageANGLE");
    glad_glPixelLocalStorageBarrierANGLE = (PFNGLPIXELLOCALSTORAGEBARRIERANGLEPROC)load("glPixelLocalStorageBarrierANGLE");
    glad_glGetFramebufferPixelLocalStorageParameterfvANGLE = (PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERFVANGLEPROC)load("glGetFramebufferPixelLocalStorageParameterfvANGLE");
    glad_glGetFramebufferPixelLocalStorageParameterivANGLE = (PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERIVANGLEPROC)load("glGetFramebufferPixelLocalStorageParameterivANGLE");
}
static void load_GL_ANGLE_polygon_mode(GLADloadproc load) {
    if(!GLAD_GL_ANGLE_polygon_mode) return;
    glad_glPolygonModeANGLE = (PFNGLPOLYGONMODEANGLEPROC)load("glPolygonModeANGLE");
}
static void load_GL_ANGLE_provoking_vertex(GLADloadproc load) {
    if(!GLAD_GL_ANGLE_provoking_vertex) return;
    glad_glProvokingVertexANGLE = (PFNGLPROVOKINGVERTEXANGLEPROC)load("glProvokingVertexANGLE");
}
static void load_GL_ARB_bindless_texture(GLADloadproc load) {
    if(!GLAD_GL_ARB_bindless_texture) return;
    glad_glGetTextureHandleARB = (PFNGLGETTEXTUREHANDLEARB)load("glGetTextureHandleARB");
    glad_glMakeTextureHandleResidentARB = (PFNGLMAKETEXTUREHANDLERESIDENTARB)load("glMakeTextureHandleResidentARB");
    glad_glMakeTextureHandleNonResidentARB = (PFNGLMAKETEXTUREHANDLENONRESIDENTARB)load("glMakeTextureHandleNonResidentARB");
}
int gladLoadCustomLoader(GLADloadproc load) {
    int ret = gladLoadGLES2Loader(load);

    const char* version = (const char*)glGetString(GL_VERSION);
    GLAD_GL_version_es = strstr(version, "OpenGL ES") != NULL;
    if (GLAD_GL_version_es)
    {
#ifdef _MSC_VER
        sscanf_s(version, "OpenGL ES %d.%d", &GLAD_GL_version_major, &GLAD_GL_version_minor);
#else
        sscanf(version, "OpenGL ES %d.%d", &GLAD_GL_version_major, &GLAD_GL_version_minor);
#endif
    }
    else
    {
#ifdef _MSC_VER
        sscanf_s(version, "%d.%d", &GLAD_GL_version_major, &GLAD_GL_version_minor);
#else
        sscanf(version, "%d.%d", &GLAD_GL_version_major, &GLAD_GL_version_minor);
#endif
    }

    GLint extensionCount;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
    for (int i = 0; i < extensionCount; ++i)
    {
        const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (strcmp(ext, "GL_ANGLE_base_vertex_base_instance_shader_builtin") == 0)
        {
            GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin = 1;
        }
        if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage") == 0)
        {
            GLAD_GL_ANGLE_shader_pixel_local_storage = 1;
        }
        else if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage_coherent") == 0)
        {
            GLAD_GL_ANGLE_shader_pixel_local_storage_coherent = 1;
        }
        else if (strcmp(ext, "GL_ANGLE_polygon_mode") == 0)
        {
            GLAD_GL_ANGLE_polygon_mode = 1;
        }
        else if (strcmp(ext, "GL_ANGLE_provoking_vertex") == 0)
        {
            GLAD_GL_ANGLE_provoking_vertex = 1;
        }
        else if (strcmp(ext, "GL_ARB_bindless_texture") == 0)
        {
            GLAD_GL_ARB_bindless_texture = 1;
        }
    }
    load_GL_ANGLE_shader_pixel_local_storage(load);
    load_GL_ANGLE_polygon_mode(load);
    load_GL_ANGLE_provoking_vertex(load);
    load_Desktop_GL(load);
    load_GL_ARB_bindless_texture(load);
    return ret;
}
