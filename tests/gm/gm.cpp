/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"

#include "common/testing_window.hpp"

using namespace rivegm;

void GM::run(std::vector<uint8_t>* pixels)
{
    TestingWindow::FrameOptions frameOptions = {.clearColor = clearColor()};
    updateFrameOptions(&frameOptions);
    auto renderer = TestingWindow::Get()->beginFrame(frameOptions);
    draw(renderer.get());
    TestingWindow::Get()->endFrame(pixels);
}

void GM::draw(rive::Renderer* renderer)
{
    renderer->save();
    this->onDraw(renderer);
    renderer->restore();
}
