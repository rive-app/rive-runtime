/*
 * Copyright 2026 Rive
 */

#pragma once

#include "ore_gm_helper.hpp"
#if ORE_GM_HAS_BACKEND
#include "rive/renderer/cmd/deferred_replayer.hpp"

#include <cassert>

namespace ore_gm
{
// The GM is handed an already open screen renderer, so beginScreenFrame just
// returns it.
class GMFrameSink : public rive::cmd::DeferredFrameSink
{
public:
    GMFrameSink(rive::gpu::RenderContext* rc,
                rive::Renderer* screen,
                OreGMContext* oreCtx,
                rive::gpu::RenderTarget* target = nullptr) :
        m_rc(rc), m_screen(screen), m_ore(oreCtx), m_target(target)
    {}
    rive::Factory* factory() override
    {
        return TestingWindow::Get()->factory();
    }
    rive::gpu::RenderContext* renderContext() override { return m_rc; }
    rive::Renderer* beginScreenFrame(uint64_t target) override
    {
        assert(target == 0);
        return m_screen;
    }
    void beginOreFrame() override { m_ore->beginFrame(m_rc); }
    void endOreFrame() override { m_ore->endFrame(m_rc); }
    void afterOreFrame() override { invalidateGLStateAfterOre(m_rc); }
    rive::gpu::RenderTarget* targetRenderTarget() override { return m_target; }

private:
    rive::gpu::RenderContext* m_rc;
    rive::Renderer* m_screen;
    OreGMContext* m_ore;
    rive::gpu::RenderTarget* m_target;
};
} // namespace ore_gm
#endif
