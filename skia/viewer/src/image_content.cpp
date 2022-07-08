/*
 * Copyright 2022 Rive
 */

#include "viewer_content.hpp"

#include "include/core/SkData.h"
#include "include/core/SkImage.h"

class ImageContent : public ViewerContent {
    sk_sp<SkImage> m_image;

public:
    ImageContent(sk_sp<SkImage> image) : m_image(std::move(image)) {}

    void handleDraw(SkCanvas* canvas, double) override { canvas->drawImage(m_image, 0, 0); }

    void handleResize(int width, int height) override {}
    void handleImgui() override {}
};

std::unique_ptr<ViewerContent> ViewerContent::Image(const char filename[]) {
    auto bytes = LoadFile(filename);
    auto data = SkData::MakeWithCopy(bytes.data(), bytes.size());
    if (auto image = SkImage::MakeFromEncoded(data)) {
        return std::make_unique<ImageContent>(std::move(image));
    }
    return nullptr;
}
