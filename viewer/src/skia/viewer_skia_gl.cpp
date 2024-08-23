#if defined(RIVE_RENDERER_SKIA) && defined(SK_GL)
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "viewer/viewer.hpp"

#include "gl/GrGLInterface.h"

sk_sp<GrDirectContext> makeSkiaContext() { return GrDirectContext::MakeGL(); }

sk_sp<SkSurface> makeSkiaSurface(GrDirectContext* context, int width, int height)
{
    GrGLFramebufferInfo framebufferInfo;
    framebufferInfo.fFBOID = 0;
    framebufferInfo.fFormat = 0x8058; // GL_RGBA8;

    GrBackendRenderTarget backendRenderTarget(width,
                                              height,
                                              0, // sample count
                                              0, // stencil bits
                                              framebufferInfo);

    return SkSurface::MakeFromBackendRenderTarget(context,
                                                  backendRenderTarget,
                                                  kBottomLeft_GrSurfaceOrigin,
                                                  kRGBA_8888_SkColorType,
                                                  nullptr,
                                                  nullptr);
}

void skiaPresentSurface(sk_sp<SkSurface> surface) {}
#endif