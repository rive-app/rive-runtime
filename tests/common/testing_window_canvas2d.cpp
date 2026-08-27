/*
 * Copyright 2022 Rive
 */
#include "testing_window.hpp"

#if !defined(RIVE_CANVAS_2D)

TestingWindow* TestingWindow::MakeCanvas2D() { return nullptr; }

#else

#include "utils/factory_utils.hpp"

#include <string>
#include <vector>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/val.h>

namespace rive::gpu
{

EM_ASYNC_JS(void, testingWindowCanvas2dImportCanvasAdvanced, (), {
    // The import itself is async, and then we need to call the "default"
    // function to get the module, which is also async.
    // clang-format off
    if (globalThis.canvasAdvancedModule === undefined)
    {
        const result = await import("./canvas_advanced.mjs");
        globalThis.canvasAdvancedModule = await result.default();
    }
    // clang-format on
});

// Resets the canvas and fills it with `color` (0xAARRGGBB). The fill has to
// happen here rather than through the renderer: a drawPath composites over the
// existing pixels, which cannot produce a transparent clear color.
EM_JS(void, testingWindowCanvas2dResetCanvas, (uint32_t color), {
    var canvas = document["getElementById"]("canvas");
    var ctx = canvas["getContext"]("2d");
    ctx["reset"]();
    // reset() leaves the canvas transparent, so compositing the clear color
    // over it yields exactly that color, alpha included.
    ctx["fillStyle"] = "rgba(" + ((0x00ff0000 & color) >>> 16) + "," +
                       ((0x0000ff00 & color) >>> 8) + "," +
                       ((0x000000ff & color) >>> 0) + "," +
                       ((0xff000000 & color) >>> 24) / 0xff + ")";
    ctx["fillRect"](0, 0, canvas["width"], canvas["height"]);
});

EM_JS(bool,
      testingWindowCanvas2dGetCanvasPixels,
      (uint8_t* outBuffer, int width, int height),
      {
          var canvas = document["getElementById"]("canvas");
          var ctx = canvas["getContext"]("2d");
          var imageData =
              ctx["getImageData"](0, 0, canvas.width, canvas.height);
          var pixelArray = imageData["data"];
          if (pixelArray.length != width * height * 4)
          {
              return false;
          }

          // TestHarness::savePNG() flips vertically on the way out, since every
          // other backend fills this buffer bottom-up (the GL convention).
          // getImageData() hands back rows top-down, so reverse them here and
          // let the two cancel.
          var stride = width * 4;
          for (var y = 0; y < height; ++y)
          {
              Module['HEAPU8']['set'](
                  pixelArray['subarray']((height - 1 - y) * stride,
                                         (height - y) * stride),
                  outBuffer + y * stride);
          }
          return true;
      });

// Image decoding is asynchronous: renderer.js hands the bytes to an <img>
// element through a blob URL and only fills in the result on its load event.
//
// Note this goes through Module.decodeImage rather than the more obvious
// renderFactory.makeRenderImage(). The latter's onDecode callback closes over
// renderer.js's loadContext, which is only non-null inside Module.load(), and
// this harness draws images directly without ever loading a .riv file, so that
// path would dereference null on the first decode.
EM_ASYNC_JS(emscripten::EM_VAL,
            testingWindowCanvas2dDecodeImage,
            (const uint8_t* bytes, int size),
            {
                // Copy out of our heap up front just to be extra safe
                // (renderer.js's `decode` function also makes a Blob copy).
                var copy = Module['HEAPU8']['slice'](bytes, bytes + size);
                var image = await new Promise(function(resolve) {
                    globalThis.canvasAdvancedModule["decodeImage"](copy,
                                                                   resolve);
                });
                return Emval.toHandle(image);
            });

// Resolved on first use rather than at static-init time, since the module isn't
// on globalThis until testingWindowCanvas2dImportCanvasAdvanced() has finished.
static emscripten::val& canvasAdvancedModule()
{
    static emscripten::val module =
        emscripten::val::global("canvasAdvancedModule");
    return module;
}

// embind exposes C++ enums to JS as value objects, not plain numbers, and
// renderer.js compares them by identity. This class maps the C++ int-based
// enums to the objects stored in canvas_advanced's heap (which is different
// from this wasm module's heap!).
//
// embind stores the reverse map on the enum's JS constructor as
// `.values[rawValue]` (see _embind_register_enum_value in libembind.js), which
// lets us build the table without naming every enumerator here.
class JSEnum
{
public:
    // Must not be constructed until canvas_advanced.mjs has finished loading.
    explicit JSEnum(const char* enumName)
    {
        emscripten::val values = canvasAdvancedModule()[enumName]["values"];
        assert(!values.isUndefined()); // Not an embind enum?

        // Index the table by raw value. Some of these enums are sparse, so the
        // gaps stay undefined and are caught by the assert in operator().
        emscripten::val keys =
            emscripten::val::global("Object").call<emscripten::val>("keys",
                                                                    values);
        size_t keyCount = keys["length"].as<size_t>();
        for (size_t i = 0; i < keyCount; i++)
        {
            int rawValue = std::stoi(keys[i].as<std::string>());
            assert(rawValue >= 0);

            // We're storing the enum mapping in a vector rather than a sparse
            // data structure, like a map. If we ever try to register an enum
            // with really high values, it might indicate that this enum is
            // meant to be used as a bit-mask, and the vector will become huge.
            // If that happens, this assert will trigger and we can switch to
            // something more appropriate.
            assert(rawValue < 1024);

            if (static_cast<size_t>(rawValue) >= m_values.size())
            {
                m_values.resize(rawValue + 1, emscripten::val::undefined());
            }
            m_values[rawValue] = values[rawValue];
        }
    }

    template <typename T> const emscripten::val& operator()(T value) const
    {
        size_t rawValue = static_cast<size_t>(value);
        assert(rawValue < m_values.size());
        assert(!m_values[rawValue].isUndefined());
        return m_values[rawValue];
    }

private:
    std::vector<emscripten::val> m_values;
};

// The names here are the ones canvas_advanced registers in
// EMSCRIPTEN_BINDINGS(RiveWASM_C2D); they are not the C++ type names.
struct JSEnums
{
    JSEnum blendMode{"BlendMode"};
    JSEnum fillRule{"FillRule"};
    JSEnum paintStyle{"RenderPaintStyle"};
    JSEnum strokeCap{"StrokeCap"};
    JSEnum strokeJoin{"StrokeJoin"};
};

// Built on first use, which must be after the import wait loop in
// TestingWindowCanvas2D's constructor.
static const JSEnums& jsEnums()
{
    static JSEnums enums;
    return enums;
}

// The classes below let this module drive the canvas2d renderer that lives in
// canvas_advanced.mjs.
//
// This harness and canvas_advanced are separately linked Emscripten modules:
// two WebAssembly.Memory instances, two C++ runtimes, two embind type
// registries. A pointer is only an offset into one module's linear memory, so
// nothing that is or contains one -- a rive::Renderer*, an rcp<>, a vtable --
// can be handed across. JS objects can, since there is only one JavaScript
// realm; that includes TypedArrays, which are ordinary JS objects that happen
// to view a particular module's heap.
//
// So each class holds the emscripten::val of its canvas_advanced counterpart
// and forwards to it, either directly for simple cases (e.g.
// m_jsPath.call<void>("moveTo", x, y)), or through Canvas2DTestUtilities, where
// that wrapper has logic worth exercising rather than duplicating.
//
// Ownership runs opposite to bindings_c2d.cpp: there the factory adopts the C++
// object out of the JS handle, whereas here the handle stays the sole owner,
// and embind never reclaims raw pointer handles by itself. So each destructor
// releases explicitly, via deleteLater() rather than delete() because
// renderer.js captures paths and paints in deferred draw-list closures that
// must outlive us until endFrame() flushes them.
//
// Every renderer object we see originates from JSRenderFactory, so a failed
// downcast is a bug rather than a legitimate "some other subclass" case --
// hence lite_rtti_cast_or_assert<> rather than the silent LITE_RTTI_CAST_OR_*
// macros.

// Gradients have no JS-side object to wrap, so there is no handle for
// makeLinearGradient() to return. Instead we hold the parameters and replay
// them onto the paint when JSRenderPaint::shader() is called, mirroring
// GradientShader in bindings_c2d.cpp.
class JSGradientShader
    : public LITE_RTTI_OVERRIDE(RenderShader, JSGradientShader)
{
public:
    JSGradientShader(const ColorInt colors[],
                     const float stops[],
                     size_t count) :
        m_stops(stops, stops + count), m_colors(colors, colors + count)
    {}

    virtual void applyToPaint(const emscripten::val& jsPaint) const = 0;

protected:
    void applyStopsToPaint(const emscripten::val& jsPaint) const
    {
        for (size_t i = 0; i < m_stops.size(); ++i)
        {
            jsPaint.call<void>("addStop", m_colors[i], m_stops[i]);
        }
    }

private:
    std::vector<float> m_stops;
    std::vector<ColorInt> m_colors;
};

class JSLinearGradientShader : public JSGradientShader
{
public:
    JSLinearGradientShader(const ColorInt colors[],
                           const float stops[],
                           size_t count,
                           float sx,
                           float sy,
                           float ex,
                           float ey) :
        JSGradientShader(colors, stops, count),
        m_startX(sx),
        m_startY(sy),
        m_endX(ex),
        m_endY(ey)
    {}

    void applyToPaint(const emscripten::val& jsPaint) const override
    {
        jsPaint.call<void>("linearGradient",
                           m_startX,
                           m_startY,
                           m_endX,
                           m_endY);
        applyStopsToPaint(jsPaint);
    }

private:
    float m_startX;
    float m_startY;
    float m_endX;
    float m_endY;
};

class JSRadialGradientShader : public JSGradientShader
{
public:
    JSRadialGradientShader(const ColorInt colors[],
                           const float stops[],
                           size_t count,
                           float cx,
                           float cy,
                           float radius) :
        JSGradientShader(colors, stops, count),
        m_centerX(cx),
        m_centerY(cy),
        m_radius(radius)
    {}

    void applyToPaint(const emscripten::val& jsPaint) const override
    {
        // renderer.js wants the radius expressed as a second point rather than
        // a scalar. Matches RadialGradientShader::passToJS in bindings_c2d.cpp.
        jsPaint.call<void>("radialGradient",
                           m_centerX,
                           m_centerY,
                           m_centerX + m_radius,
                           m_centerY);
        applyStopsToPaint(jsPaint);
    }

private:
    float m_centerX;
    float m_centerY;
    float m_radius;
};

class JSRenderPath : public LITE_RTTI_OVERRIDE(RenderPath, JSRenderPath)
{
public:
    JSRenderPath(emscripten::val&& jsPath) :
        m_jsPath(std::forward<emscripten::val>(jsPath))
    {}

    // Queued rather than deleted outright: renderer.js's _drawPath/_clipPath
    // capture the path in deferred draw-list closures, so it has to outlive us
    // until TestingWindowCanvas2D::endFrame() flushes them. See the comment on
    // flushPendingDeletes() there.
    ~JSRenderPath() override { m_jsPath.call<void>("deleteLater"); }

    void rewind() override { m_jsPath.call<void>("rewind"); }

    void fillRule(FillRule value) override
    {
        m_jsPath.call<void>("fillRule", jsEnums().fillRule(value));
    }

    void moveTo(float x, float y) override
    {
        m_jsPath.call<void>("moveTo", x, y);
    }

    void lineTo(float x, float y) override
    {
        m_jsPath.call<void>("lineTo", x, y);
    }

    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override
    {
        m_jsPath.call<void>("cubicTo", ox, oy, ix, iy, x, y);
    }

    void close() override { m_jsPath.call<void>("close"); }

    void addRenderPath(const RenderPath* path, const Mat2D& transform) override
    {
        float xx = transform.xx();
        float xy = transform.xy();
        float yx = transform.yx();
        float yy = transform.yy();
        float tx = transform.tx();
        float ty = transform.ty();
        auto* jsPath =
            rive::lite_rtti_cast_or_assert<const JSRenderPath*>(path);
        m_jsPath.call<void>("addPath", jsPath->js(), xx, xy, yx, yy, tx, ty);
    }

    void addRawPath(const RawPath& path) override
    {
        const rive::Vec2D* pts = path.points().data();
        for (auto v : path.verbs())
        {
            switch ((rive::PathVerb)v)
            {
                case rive::PathVerb::move:
                    move(*pts++);
                    break;
                case rive::PathVerb::line:
                    line(*pts++);
                    break;
                case rive::PathVerb::cubic:
                    cubic(pts[0], pts[1], pts[2]);
                    pts += 3;
                    break;
                case rive::PathVerb::close:
                    close();
                    break;
                default:
                    assert(false); // unexpected verb
            }
        }
        assert(pts - path.points().data() == path.points().size());
    }

    emscripten::val& js() { return m_jsPath; }
    const emscripten::val& js() const { return m_jsPath; }

private:
    emscripten::val m_jsPath;
};

class JSRenderPaint : public LITE_RTTI_OVERRIDE(RenderPaint, JSRenderPaint)
{
public:
    JSRenderPaint(emscripten::val&& jsPaint) :
        m_jsPaint(std::forward<emscripten::val>(jsPaint))
    {}

    ~JSRenderPaint() override { m_jsPaint.call<void>("deleteLater"); }

    void style(RenderPaintStyle style) override
    {
        m_jsPaint.call<void>("style", jsEnums().paintStyle(style));
    }

    void color(ColorInt value) override
    {
        m_jsPaint.call<void>("color", value);
    }

    void thickness(float value) override
    {
        m_jsPaint.call<void>("thickness", value);
    }

    void join(StrokeJoin value) override
    {
        m_jsPaint.call<void>("join", jsEnums().strokeJoin(value));
    }

    void cap(StrokeCap value) override
    {
        m_jsPaint.call<void>("cap", jsEnums().strokeCap(value));
    }

    void feather(float value) override
    {
        // Not currently implemented (yet)
    }

    void blendMode(BlendMode value) override
    {
        m_jsPaint.call<void>("blendMode", jsEnums().blendMode(value));
    }

    void shader(rcp<RenderShader> shader) override
    {
        if (shader == nullptr)
        {
            m_jsPaint.call<void>("clearGradient");
            return;
        }

        rive::lite_rtti_cast_or_assert<JSGradientShader*>(shader.get())
            ->applyToPaint(m_jsPaint);
    }

    void invalidateStroke() override {}

    emscripten::val& js() { return m_jsPaint; }
    const emscripten::val& js() const { return m_jsPaint; }

private:
    emscripten::val m_jsPaint;
};

class JSRenderImage : public LITE_RTTI_OVERRIDE(RenderImage, JSRenderImage)
{
public:
    JSRenderImage(emscripten::val&& jsImage) :
        m_jsImage(std::forward<emscripten::val>(jsImage))
    {
        // On load, renderer.js calls size() on the RenderImage it created,
        // which sets the dimensions on the wrapper in canvas_advanced's heap.
        // Ours is a separate rive::RenderImage with its own m_Width/m_Height,
        // and rive core reads those, so copy them across.
        emscripten::val testUtils =
            canvasAdvancedModule()["Canvas2DTestUtilities"];
        m_Width = testUtils.call<int>("imageWidth", m_jsImage);
        m_Height = testUtils.call<int>("imageHeight", m_jsImage);
    }

    ~JSRenderImage() override { m_jsImage.call<void>("deleteLater"); }

    emscripten::val& js() { return m_jsImage; }
    const emscripten::val& js() const { return m_jsImage; }

private:
    emscripten::val m_jsImage;
};

class JSRenderFactory : public rive::Factory
{
public:
    JSRenderFactory() : m_jsFactory(canvasAdvancedModule()["renderFactory"]) {}

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType type,
                                       RenderBufferFlags flags,
                                       size_t sizeInBytes) override
    {
        // These never cross the module boundary: rive core maps and fills them
        // here, and JSRenderer::drawImageMesh() copies the contents over when
        // it hands off. Must be a DataRenderBuffer for the LITE_RTTI casts
        // there to succeed.
        return make_rcp<DataRenderBuffer>(type, flags, sizeInBytes);
    }

    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override
    {
        return rcp<RenderShader>(
            new JSLinearGradientShader(colors, stops, count, sx, sy, ex, ey));
    }

    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override
    {
        return rcp<RenderShader>(
            new JSRadialGradientShader(colors, stops, count, cx, cy, radius));
    }

    rcp<RenderPath> makeRenderPath(RawPath& rawPath, FillRule fillRule) override
    {
        rcp<RenderPath> renderPath = makeEmptyRenderPath();
        renderPath->addRawPath(rawPath);
        renderPath->fillRule(fillRule);
        return renderPath;
    }

    rcp<RenderPath> makeEmptyRenderPath() override
    {
        return make_rcp<JSRenderPath>(
            m_jsFactory.call<emscripten::val>("makeRenderPath"));
    }

    rcp<RenderPaint> makeRenderPaint() override
    {
        return make_rcp<JSRenderPaint>(
            m_jsFactory.call<emscripten::val>("makeRenderPaint"));
    }

    rcp<RenderImage> decodeImage(Span<const uint8_t> bytes) override
    {
        if (bytes.empty())
        {
            return nullptr;
        }

        // Blocks until the <img> load event fires.
        return make_rcp<JSRenderImage>(emscripten::val::take_ownership(
            testingWindowCanvas2dDecodeImage(bytes.data(),
                                             static_cast<int>(bytes.size()))));
    }

private:
    emscripten::val m_jsFactory;
};

class JSRenderer : public rive::Renderer
{
public:
    JSRenderer() :
        m_jsRenderer(canvasAdvancedModule().call<emscripten::val>(
            "makeRenderer",
            emscripten::val::global("document")
                .call<emscripten::val>("getElementById",
                                       std::string("canvas"))))
    {}

    // Queued, not deleted: renderer.js holds us in _pendingCanvasRenderers
    // until the draw list is flushed.
    ~JSRenderer() override { m_jsRenderer.call<void>("deleteLater"); }

    void beginFrame(bool clear)
    {
        m_jsRenderer.call<void>("beginFrame", clear);
    }

    void save() override { m_jsRenderer.call<void>("save"); }
    void restore() override { m_jsRenderer.call<void>("restore"); }

    void transform(const Mat2D& matrix) override
    {
        m_jsRenderer.call<void>("transform",
                                matrix.xx(),
                                matrix.xy(),
                                matrix.yx(),
                                matrix.yy(),
                                matrix.tx(),
                                matrix.ty());
    }

    void drawPath(RenderPath* path, RenderPaint* paint) override
    {
        auto* jsPath = rive::lite_rtti_cast_or_assert<JSRenderPath*>(path);
        auto* jsPaint = rive::lite_rtti_cast_or_assert<JSRenderPaint*>(paint);
        m_jsRenderer.call<void>("_drawPath", jsPath->js(), jsPaint->js());
    }

    void clipPath(RenderPath* path) override
    {
        auto* jsPath = rive::lite_rtti_cast_or_assert<JSRenderPath*>(path);
        m_jsRenderer.call<void>("_clipPath", jsPath->js());
    }

    void drawImage(const RenderImage* image,
                   ImageSampler sampler,
                   BlendMode blendMode,
                   float opacity) override
    {
        auto* jsImage =
            rive::lite_rtti_cast_or_assert<const JSRenderImage*>(image);
        m_jsRenderer.call<void>("_drawRiveImage",
                                jsImage->js(),
                                jsEnums().blendMode(blendMode),
                                opacity);
    }

    void drawImageMesh(const RenderImage* image,
                       ImageSampler sampler,
                       rcp<RenderBuffer> vertices_f32,
                       rcp<RenderBuffer> uvCoords_f32,
                       rcp<RenderBuffer> indices_u16,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode blendMode,
                       float opacity) override
    {
        auto* vtx = rive::lite_rtti_cast_or_assert<rive::DataRenderBuffer*>(
            vertices_f32.get());
        auto* uv = rive::lite_rtti_cast_or_assert<rive::DataRenderBuffer*>(
            uvCoords_f32.get());
        auto* indices = rive::lite_rtti_cast_or_assert<rive::DataRenderBuffer*>(
            indices_u16.get());

        uint32_t f32Count = vertexCount * 2;
        assert(vtx->sizeInBytes() == f32Count * sizeof(float));
        assert(uv->sizeInBytes() == f32Count * sizeof(float));
        assert(indices->sizeInBytes() == indexCount * sizeof(uint16_t));

        if (f32Count == 0 || indexCount == 0)
        {
            return;
        }

        // Unlike the rest of the shims, this one hands off to the C++ side of
        // canvas_advanced rather than to renderer.js, so that the real
        // RendererWrapper::drawImageMesh() runs -- it owns the mesh bounding
        // box computation and the atlas packing, and we'd otherwise have to
        // duplicate them here (and leave them untested).
        //
        // The typed_memory_views below are over our heap. A TypedArray is a
        // plain JS object, so canvas_advanced can read it even though it wraps
        // a different ArrayBuffer; it copies into its own DataRenderBuffers.
        // Nothing between here and the call allocates locally, so our memory
        // can't grow and detach them in the meantime.
        auto* jsImage =
            rive::lite_rtti_cast_or_assert<const JSRenderImage*>(image);
        canvasAdvancedModule()["Canvas2DTestUtilities"].call<void>(
            "drawImageMesh",
            m_jsRenderer,
            jsImage->js(),
            emscripten::val{
                emscripten::typed_memory_view(f32Count, vtx->f32s())},
            emscripten::val{
                emscripten::typed_memory_view(f32Count, uv->f32s())},
            emscripten::val{
                emscripten::typed_memory_view(indexCount, indices->u16s())},
            jsEnums().blendMode(blendMode),
            opacity);
    }

    void modulateOpacity(float opacity) override
    {
        m_jsRenderer.call<void>("modulateOpacity", opacity);
    }

private:
    emscripten::val m_jsRenderer;
};

class TestingWindowCanvas2D : public TestingWindow
{
public:
    TestingWindowCanvas2D()
    {
        testingWindowCanvas2dImportCanvasAdvanced();

        int w, h;
        emscripten_get_canvas_element_size("#canvas", &w, &h);
        m_width = w;
        m_height = h;

        m_factory = std::make_unique<JSRenderFactory>();
    }

    rive::Factory* factory() override { return m_factory.get(); }

    void resize(int width, int height) override
    {
        if (m_width != width || m_height != height)
        {
            TestingWindow::resize(width, height);
            emscripten_set_canvas_element_size("#canvas", width, height);
        }
    }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        if (options.doClear)
        {
            testingWindowCanvas2dResetCanvas(options.clearColor);
        }

        auto renderer = std::make_unique<JSRenderer>();
        renderer->beginFrame(false);

        return renderer;
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        // Flush commands so that we can read pixel data from the canvas.
        canvasAdvancedModule().call<void>("resolveAnimationFrame");

        // Now that the deferred draw list has run, nothing on the JS side is
        // still holding the objects our shims queued via deleteLater(). Embind
        // never reclaims these on its own -- class handles created from JS own
        // their C++ instance outright, and no finalizer is attached to raw
        // pointer handles -- so without this every path, paint and renderer
        // would accumulate in canvas_advanced's heap for the whole run.
        canvasAdvancedModule().call<void>("flushPendingDeletes");

        if (!pixelData)
        {
            return;
        }

        pixelData->resize(m_width * m_height * 4);
        if (!testingWindowCanvas2dGetCanvasPixels(pixelData->data(),
                                                  m_width,
                                                  m_height))
        {
            printf("Canvas size mismatch, read failed\n");
            pixelData->assign(pixelData->size(), 0);
            return;
        }

        // getImageData() hands back unpremultiplied RGBA. Multiply by alpha to
        // match the other backends.
        for (size_t i = 0; i < pixelData->size(); i += 4)
        {
            uint32_t a = (*pixelData)[i + 3];
            for (size_t c = 0; c < 3; ++c)
            {
                (*pixelData)[i + c] =
                    static_cast<uint8_t>(((*pixelData)[i + c] * a + 127) / 255);
            }
        }
    }

private:
    std::unique_ptr<JSRenderFactory> m_factory;
};
}; // namespace rive::gpu

TestingWindow* TestingWindow::MakeCanvas2D()
{
    return new rive::gpu::TestingWindowCanvas2D();
}

#endif
