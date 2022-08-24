/*
 * Copyright 2022 Rive
 */

#include "viewer/viewer_content.hpp"
#include "rive/factory.hpp"
#include "rive/renderer.hpp"

class ImageContent : public ViewerContent {
    std::unique_ptr<rive::RenderImage> m_image;

public:
    ImageContent(std::unique_ptr<rive::RenderImage> image) : m_image(std::move(image)) {}

    void handleDraw(rive::Renderer* renderer, double) override {
        renderer->drawImage(m_image.get(), rive::BlendMode::srcOver, 1);
    }

    void handleResize(int width, int height) override {}
    void handleImgui() override {}
};

std::unique_ptr<ViewerContent> ViewerContent::Image(const char filename[]) {
    auto bytes = LoadFile(filename);
    auto image = RiveFactory()->decodeImage(bytes);
    if (image) {
        return std::make_unique<ImageContent>(std::move(image));
    }
    return nullptr;
}
