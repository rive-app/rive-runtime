#ifndef _RIVE_VIEWER_HPP_
#define _RIVE_VIEWER_HPP_

#ifdef RIVE_RENDERER_SKIA
#include "GrBackendSurface.h"
#include "GrDirectContext.h"
#include "SkCanvas.h"
#include "SkColorSpace.h"
#include "SkSurface.h"
#include "SkTypes.h"

sk_sp<GrDirectContext> makeSkiaContext();
sk_sp<SkSurface> makeSkiaSurface(GrDirectContext* context, int width, int height);
void skiaPresentSurface(sk_sp<SkSurface> surface);
#endif

// Helper to ensure the gl context is currently bound.
void bindGraphicsContext();

#endif