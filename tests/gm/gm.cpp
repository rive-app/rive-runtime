/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"

#include "gmutils.hpp"

using namespace rivegm;

template GMRegistry* GMRegistry::s_Head;

void GM::run(std::vector<uint8_t>* pixels)
{
    auto renderer = TestingWindow::Get()->beginFrame(clearColor());
    draw(renderer.get());
    TestingWindow::Get()->endFrame(pixels);
}

void GM::draw(rive::Renderer* renderer)
{
    renderer->save();
    this->onDraw(renderer);
    renderer->restore();
}
