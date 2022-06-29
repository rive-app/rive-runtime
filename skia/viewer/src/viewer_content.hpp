/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_VIEWER_CONTENT_HPP_
#define _RIVE_VIEWER_CONTENT_HPP_

#include "rive/span.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkSize.h"
#include "imgui/imgui.h"

class ViewerContent {
public:
    virtual ~ViewerContent() {}

    virtual void handleResize(int width, int height) = 0;
    virtual void handleDraw(SkCanvas* canvas, double elapsed) = 0;
    virtual void handleImgui() = 0;

    virtual void handlePointerMove(float x, float y) {}
    virtual void handlePointerDown() {}
    virtual void handlePointerUp() {}

    using Factory = std::unique_ptr<ViewerContent> (*)(const char filename[],
                                                       rive::Span<const uint8_t>);

    // Searches all handlers and returns a content if it is found.
    static std::unique_ptr<ViewerContent> FindHandler(const char filename[],
                                                      rive::Span<const uint8_t> data) {
        Factory factories[] = { Scene, Image };
        for (auto f : factories) {
            if (auto content = f(filename, data)) {
                return content;
            }
        }
        return nullptr;
    }

    // Private factories...
    static std::unique_ptr<ViewerContent> Scene(const char[], rive::Span<const uint8_t>);
    static std::unique_ptr<ViewerContent> Image(const char[], rive::Span<const uint8_t>);
};

#endif
