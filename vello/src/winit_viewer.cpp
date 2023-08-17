#include "viewer/viewer_content.hpp"

#include "vello_renderer.hpp"

extern "C"
{
    typedef void* RawViewerContent;

    const RawViewerContent viewer_content_new(const char* filename)
    {
        return static_cast<void*>(ViewerContent::findHandler(filename).release());
    }

    void viewer_content_release(const RawViewerContent viewer_content)
    {
        std::unique_ptr<ViewerContent> val(std::move(static_cast<ViewerContent*>(viewer_content)));
    }

    void viewer_content_handle_resize(const RawViewerContent viewer_content,
                                      int32_t width,
                                      int32_t height)
    {
        static_cast<ViewerContent*>(viewer_content)->handleResize(width, height);
    }

    void viewer_content_handle_draw(const RawViewerContent viewer_content,
                                    RawVelloRenderer raw_renderer,
                                    double elapsed)
    {
        VelloRenderer renderer = VelloRenderer(raw_renderer);
        static_cast<ViewerContent*>(viewer_content)->handleDraw(&renderer, elapsed);
    }

    void viewer_content_handle_pointer_move(const RawViewerContent viewer_content, float x, float y)
    {
        static_cast<ViewerContent*>(viewer_content)->handlePointerMove(x, y);
    }

    void viewer_content_handle_pointer_down(const RawViewerContent viewer_content, float x, float y)
    {
        static_cast<ViewerContent*>(viewer_content)->handlePointerDown(x, y);
    }

    void viewer_content_handle_pointer_up(const RawViewerContent viewer_content, float x, float y)
    {
        static_cast<ViewerContent*>(viewer_content)->handlePointerUp(x, y);
    }
}
