#ifndef _RIVE_WASM_MODULE_RENDER_HPP_
#define _RIVE_WASM_MODULE_RENDER_HPP_

#ifdef RIVE_WASM_MODULE

#include "rive/refcnt.hpp"

#include <memory>
#include <stdint.h>

namespace rive
{
class Factory;
class Renderer;
class RenderImage;
namespace ore
{
class Context;
}

/// The script module's handle-backed render factory; every object it makes
/// crosses to the host as a u32 handle through the rive_*_v1 imports.
Factory* wasmModuleFactory();

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
namespace ore
{
class TextureView;
}
namespace gpu
{
class RenderCanvas;
}
/// The module's ore context; factories realize against rive_gpu_v1 handles.
ore::Context* wasmModuleOreContext();

/// Module-side canvas pair over a host canvas handle: a RenderCanvas whose
/// target owns the handle, and a color view carrying real attachment
/// metadata for pass validation.
struct WasmModuleCanvas
{
    rcp<gpu::RenderCanvas> canvas;
    rcp<ore::TextureView> colorView;
};
WasmModuleCanvas wasmModuleWrapCanvas(uint32_t canvasHandle);
/// Recreates the wrapped canvas at a new size through the host; empty on
/// failure. The canvas handle transfers to the returned wrap.
WasmModuleCanvas wasmModuleResizeCanvas(const rcp<gpu::RenderCanvas>& canvas,
                                        uint32_t width,
                                        uint32_t height);
/// Mints a fresh host image handle over the canvas's presentable image.
uint32_t wasmModuleCanvasImageHandle(const rcp<gpu::RenderCanvas>& canvas);
#endif

/// Wraps a host renderer handle for one draw call.
std::unique_ptr<Renderer> makeWasmModuleRenderer(uint32_t handle);

/// The host handle inside a module renderer.
uint32_t wasmModuleRendererHandle(Renderer* renderer);

/// Wraps a host image handle; dimensions are fetched once at wrap time.
rcp<RenderImage> makeWasmModuleRenderImage(uint32_t handle);

/// The host handle inside a module render image.
uint32_t wasmModuleImageHandle(RenderImage* image);
} // namespace rive

#endif
#endif
