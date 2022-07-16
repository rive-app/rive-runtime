/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_VIEWER_CONTENT_HPP_
#define _RIVE_VIEWER_CONTENT_HPP_

#include "rive/span.hpp"
#include "imgui.h"

#ifdef RIVE_RENDERER_SKIA
#include "SkCanvas.h"
#include "viewer/skia/viewer_skia_renderer.hpp"
#endif

namespace rive {
    class Renderer;
    class Factory;
} // namespace rive

class ViewerContent {
public:
    virtual ~ViewerContent();

    virtual void handleResize(int width, int height) = 0;
    virtual void handleDraw(rive::Renderer* renderer, double elapsed) = 0;
    virtual void handleImgui() = 0;

    virtual void handlePointerMove(float x, float y) {}
    virtual void handlePointerDown(float x, float y) {}
    virtual void handlePointerUp(float x, float y) {}

    using Factory = std::unique_ptr<ViewerContent> (*)(const char filename[]);

    // Searches all handlers and returns a content if it is found.
    static std::unique_ptr<ViewerContent> findHandler(const char filename[]) {
        Factory factories[] = {
            Scene,
#ifdef RIVE_RENDERER_SKIA
            Image,
            Text,
            TextPath,
#endif
        };
        for (auto f : factories) {
            if (auto content = f(filename)) {
                return content;
            }
        }
        return nullptr;
    }

    // Private factories...
    static std::unique_ptr<ViewerContent> Scene(const char[]);
#ifdef RIVE_RENDERER_SKIA
    // Helper to get the canvas from a rive::Renderer. We know that when we're
    // using the skia renderer our viewer always creates a skia renderer.
    SkCanvas* skiaCanvas(rive::Renderer* renderer) {
        return static_cast<ViewerSkiaRenderer*>(renderer)->canvas();
    }
    static std::unique_ptr<ViewerContent> Image(const char[]);
    static std::unique_ptr<ViewerContent> Text(const char[]);
    static std::unique_ptr<ViewerContent> TextPath(const char[]);
#endif

    static std::vector<uint8_t> LoadFile(const char path[]);
    static void DumpCounters(const char label[]);

    // Abstracts which rive Factory is currently used.
    static rive::Factory* RiveFactory();
};

#endif
