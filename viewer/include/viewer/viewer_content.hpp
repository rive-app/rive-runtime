/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_VIEWER_CONTENT_HPP_
#define _RIVE_VIEWER_CONTENT_HPP_

#include "rive/span.hpp"
#include "rive/refcnt.hpp"

#include "imgui.h"

namespace rive {
class Renderer;
class Factory;
class RenderFont;
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
            Image,
            Text,
            TextPath,
        };
        for (auto f : factories) {
            if (auto content = f(filename)) {
                return content;
            }
        }
        return nullptr;
    }

    // Private factories...
    static std::unique_ptr<ViewerContent> Image(const char[]);
    static std::unique_ptr<ViewerContent> Scene(const char[]);
    static std::unique_ptr<ViewerContent> Text(const char[]);
    static std::unique_ptr<ViewerContent> TextPath(const char[]);
    static std::unique_ptr<ViewerContent> TrimPath(const char[]);

    static std::vector<uint8_t> LoadFile(const char path[]);
    static void DumpCounters(const char label[]);

    // Abstracts which rive Factory is currently used.
    static rive::Factory* RiveFactory();

    // Abstracts which font backend is currently used.
    static rive::rcp<rive::RenderFont> DecodeFont(rive::Span<const uint8_t>);
};

#endif
