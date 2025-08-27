#pragma once

#include <glad/gles2.h>

#ifdef __cplusplus
extern "C" {
#endif

int gladLoadCustomLoader(GLADloadfunc);

GLAPI int GLAD_GL_version_es;
GLAPI int GLAD_GL_version_major;
GLAPI int GLAD_GL_version_minor;

#define GLAD_IS_GL_VERSION_AT_LEAST(MAJOR, MINOR) \
    ((GLAD_GL_version_major << 16) | GLAD_GL_version_minor) >= (((MAJOR) << 16) | (MINOR))

// Manual additions for extensions not supported in the glad tool.
#ifndef GL_ANGLE_base_vertex_base_instance_shader_builtin
#define GL_ANGLE_base_vertex_base_instance_shader_builtin 1
GLAPI int GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin;
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
GLAPI int GLAD_GL_ANGLE_shader_pixel_local_storage;
GLAPI int GLAD_GL_ANGLE_shader_pixel_local_storage_coherent;
typedef void (GLAD_API_PTR* PFNGLFRAMEBUFFERMEMORYLESSPIXELLOCALSTORAGEANGLEPROC) (GLint plane, GLenum internalformat);
GLAPI PFNGLFRAMEBUFFERMEMORYLESSPIXELLOCALSTORAGEANGLEPROC glad_glFramebufferMemorylessPixelLocalStorageANGLE;
#define glFramebufferMemorylessPixelLocalStorageANGLE glad_glFramebufferMemorylessPixelLocalStorageANGLE
typedef void (GLAD_API_PTR* PFNGLFRAMEBUFFERTEXTUREPIXELLOCALSTORAGEANGLEPROC) (GLint plane, GLuint backingtexture, GLint level, GLint layer);
GLAPI PFNGLFRAMEBUFFERTEXTUREPIXELLOCALSTORAGEANGLEPROC glad_glFramebufferTexturePixelLocalStorageANGLE;
#define glFramebufferTexturePixelLocalStorageANGLE glad_glFramebufferTexturePixelLocalStorageANGLE
typedef void (GLAD_API_PTR* PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEFVANGLEPROC) (GLint plane, const GLfloat value[4]);
GLAPI PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEFVANGLEPROC glad_glFramebufferPixelLocalClearValuefvANGLE;
#define glFramebufferPixelLocalClearValuefvANGLE glad_glFramebufferPixelLocalClearValuefvANGLE
typedef void (GLAD_API_PTR* PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEIVANGLEPROC) (GLint plane, const GLint value[4]);
GLAPI PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEIVANGLEPROC glad_glFramebufferPixelLocalClearValueivANGLE;
#define glFramebufferPixelLocalClearValueivANGLE glad_glFramebufferPixelLocalClearValueivANGLE
typedef void (GLAD_API_PTR* PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEUIVANGLEPROC) (GLint plane, const GLuint value[4]);
GLAPI PFNGLFRAMEBUFFERPIXELLOCALCLEARVALUEUIVANGLEPROC glad_glFramebufferPixelLocalClearValueuivANGLE;
#define glFramebufferPixelLocalClearValueuivANGLE glad_glFramebufferPixelLocalClearValueuivANGLE
typedef void (GLAD_API_PTR* PFNGLBEGINPIXELLOCALSTORAGEANGLEPROC) (GLsizei n, const GLenum loadops[]);
GLAPI PFNGLBEGINPIXELLOCALSTORAGEANGLEPROC glad_glBeginPixelLocalStorageANGLE;
#define glBeginPixelLocalStorageANGLE glad_glBeginPixelLocalStorageANGLE
typedef void (GLAD_API_PTR* PFNGLENDPIXELLOCALSTORAGEANGLEPROC) (GLsizei n, const GLenum storeops[]);
GLAPI PFNGLENDPIXELLOCALSTORAGEANGLEPROC glad_glEndPixelLocalStorageANGLE;
#define glEndPixelLocalStorageANGLE glad_glEndPixelLocalStorageANGLE
typedef void (GLAD_API_PTR* PFNGLPIXELLOCALSTORAGEBARRIERANGLEPROC) ();
GLAPI PFNGLPIXELLOCALSTORAGEBARRIERANGLEPROC glad_glPixelLocalStorageBarrierANGLE;
#define glPixelLocalStorageBarrierANGLE glad_glPixelLocalStorageBarrierANGLE
typedef void (GLAD_API_PTR* PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERFVANGLEPROC) (GLint plane, GLenum pname, GLfloat* params);
GLAPI PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERFVANGLEPROC glad_glGetFramebufferPixelLocalStorageParameterfvANGLE;
#define glGetFramebufferPixelLocalStorageParameterfvANGLE glad_glGetFramebufferPixelLocalStorageParameterfvANGLE
typedef void (GLAD_API_PTR* PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERIVANGLEPROC) (GLint plane, GLenum pname, GLint* params);
GLAPI PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGEPARAMETERIVANGLEPROC glad_glGetFramebufferPixelLocalStorageParameterivANGLE;
#define glGetFramebufferPixelLocalStorageParameterivANGLE glad_glGetFramebufferPixelLocalStorageParameterivANGLE
#endif /* GL_ANGLE_shader_pixel_local_storage */

#ifndef GL_ANGLE_polygon_mode
#define GL_ANGLE_polygon_mode 1
#define GL_POLYGON_MODE_ANGLE 0x0B40
#define GL_POLYGON_OFFSET_LINE_ANGLE 0x2A02
#define GL_LINE_ANGLE 0x1B01
#define GL_FILL_ANGLE 0x1B02
GLAPI int GLAD_GL_ANGLE_polygon_mode;
typedef void (GLAD_API_PTR* PFNGLPOLYGONMODEANGLEPROC) (GLenum face, GLenum mode);
GLAPI PFNGLPOLYGONMODEANGLEPROC glad_glPolygonModeANGLE;
#define glPolygonModeANGLE glad_glPolygonModeANGLE
#endif /* GL_ANGLE_polygon_mode */

#ifndef GL_ANGLE_provoking_vertex
#define GL_ANGLE_provoking_vertex 1
#define GL_FIRST_VERTEX_CONVENTION_ANGLE 0x8E4D
#define GL_LAST_VERTEX_CONVENTION_ANGLE 0x8E4E
#define GL_PROVOKING_VERTEX_ANGLE 0x8E4F
GLAPI int GLAD_GL_ANGLE_provoking_vertex;
typedef void (GLAD_API_PTR* PFNGLPROVOKINGVERTEXANGLEPROC) (GLenum provokeMode);
GLAPI PFNGLPROVOKINGVERTEXANGLEPROC glad_glProvokingVertexANGLE;
#define glProvokingVertexANGLE glad_glProvokingVertexANGLE
#endif  /* GL_ANGLE_provoking_vertex */

#ifdef __cplusplus
}
#endif
