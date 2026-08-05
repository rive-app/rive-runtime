/*
 * Copyright 2026 Rive
 */

#pragma once

#include <cstdint>

// Hook the scripting layer uses when a deferred host is recording. Stored on
// the ScriptingContext as a forward declared pointer so the script headers
// stay decoupled from the cmd layer; DeferredSession implements it.
namespace rive
{
class Renderer;
namespace gpu
{
class RenderCanvas;
}

namespace cmd
{

class DeferredCanvasHost
{
public:
    virtual ~DeferredCanvasHost() = default;

    // Emits the content begin bracket and returns a recording renderer owned
    // by the host and valid until endCanvasContent. clearColor is ARGB.
    virtual Renderer* beginCanvasContent(gpu::RenderCanvas* canvas,
                                         uint32_t clearColor) = 0;

    // Emits the content end bracket and releases the renderer.
    virtual void endCanvasContent(gpu::RenderCanvas* canvas) = 0;
};

} // namespace cmd
} // namespace rive
