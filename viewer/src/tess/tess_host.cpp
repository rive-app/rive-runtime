/*
 * Copyright 2022 Rive
 */

#include "viewer/viewer_host.hpp"
#include "viewer/viewer_content.hpp"

#ifdef RIVE_RENDERER_TESS

#include "rive/tess/sokol/sokol_tess_renderer.hpp"
#include "viewer/tess/viewer_sokol_factory.hpp"

class TessViewerHost : public ViewerHost
{
public:
    std::unique_ptr<rive::SokolTessRenderer> m_renderer;

    bool init(sg_pass_action*, int width, int height) override
    {
        m_renderer = rivestd::make_unique<rive::SokolTessRenderer>();
        m_renderer->orthographicProjection(0.0f, width, height, 0.0f, 0.0f, 1.0f);
        return true;
    }

    void handleResize(int width, int height) override
    {
        m_renderer->orthographicProjection(0.0f, width, height, 0.0f, 0.0f, 1.0f);
    }

    void afterDefaultPass(ViewerContent* content, double elapsed) override
    {
        m_renderer->reset();
        if (content)
        {
            content->handleDraw(m_renderer.get(), elapsed);
        }
    }
};

std::unique_ptr<ViewerHost> ViewerHost::Make() { return rivestd::make_unique<TessViewerHost>(); }

rive::Factory* ViewerHost::Factory()
{
    static ViewerSokolFactory sokolFactory;
    return &sokolFactory;
}

#endif
