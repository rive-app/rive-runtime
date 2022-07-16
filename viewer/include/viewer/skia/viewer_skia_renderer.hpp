#ifndef _RIVE_VIEWER_SKIA_RENDERER_HPP_
#define _RIVE_VIEWER_SKIA_RENDERER_HPP_

#ifdef RIVE_RENDERER_SKIA
#include "skia_renderer.hpp"

class ViewerSkiaRenderer : public rive::SkiaRenderer {
public:
    ViewerSkiaRenderer(SkCanvas* canvas) : rive::SkiaRenderer(canvas) {}
    SkCanvas* canvas() { return m_Canvas; }
};
#endif
#endif