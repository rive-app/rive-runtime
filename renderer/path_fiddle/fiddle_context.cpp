/*
 * Copyright 2026 Rive
 */

#include "fiddle_context.hpp"

#include "rive/renderer/ore/ore_context.hpp"

void FiddleContext::beginOreFrame(rive::ore::Context* oreContext)
{
    oreContext->beginFrame({.externalCommandBuffer = getCommandBuffer()});
}

void FiddleContext::endOreFrame(rive::ore::Context* oreContext)
{
    oreContext->endFrame();
}
