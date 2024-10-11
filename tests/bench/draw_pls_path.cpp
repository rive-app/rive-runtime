/*
 * Copyright 2022 Rive
 */

#include "bench.hpp"

#include "common/render_context_null.hpp"
#include "rive/file.hpp"
#include "rive/scene.hpp"
#include "assets/paper.riv.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive_render_paint.hpp"
#include "rive_render_path.hpp"

using namespace rive;
using namespace rive::gpu;

using PathDraw = std::tuple<rcp<RiveRenderPath>, rcp<RiveRenderPaint>>;

// Measure the speed of RiveRenderer::drawPath(), with a null render context.
class DrawRiveRenderPath : public Bench
{
protected:
    int run() const final
    {
        for (int i = 0; i < 10; ++i)
        {
            m_nullContext->beginFrame({
                .renderTargetWidth = m_renderTarget->width(),
                .renderTargetHeight = m_renderTarget->height(),
            });
            for (const auto& [path, paint] : m_pathDraws)
            {
                m_renderer.drawPath(path.get(), paint.get());
            }
            m_nullContext->flush({.renderTarget = m_renderTarget.get()});
        }
        return 0;
    }

    std::vector<PathDraw> m_pathDraws;
    std::unique_ptr<RenderContext> m_nullContext =
        RenderContextNULL::MakeContext();
    mutable RiveRenderer m_renderer{m_nullContext.get()};
    rive::rcp<RenderTarget> m_renderTarget =
        m_nullContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
            1600,
            1600);
};

class SniffPathsRenderer : public Renderer
{
public:
    SniffPathsRenderer(std::vector<PathDraw>* pathDraws,
                       bool allStrokes,
                       bool allRoundJoin) :
        m_pathDraws(pathDraws),
        m_allStrokes(allStrokes),
        m_allRoundJoin(allRoundJoin)
    {}
    void save() override {}
    void restore() override {}
    void transform(const Mat2D& matrix) override {}
    void drawPath(RenderPath* path, RenderPaint* paint) override
    {
        auto renderPath = static_cast<RiveRenderPath*>(path);
        auto renderPaint = static_cast<RiveRenderPaint*>(paint);
        if (m_allStrokes)
        {
            renderPaint->style(RenderPaintStyle::stroke);
            renderPaint->join(StrokeJoin::bevel);
            renderPaint->thickness(2);
        }
        if (m_allRoundJoin)
        {
            renderPaint->join(StrokeJoin::round);
        }

        m_pathDraws->push_back({ref_rcp(renderPath), ref_rcp(renderPaint)});
    }
    void clipPath(RenderPath* path) override {}
    void drawImage(const RenderImage*, BlendMode, float) override {}
    void drawImageMesh(const RenderImage*,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode,
                       float) override
    {}

private:
    std::vector<PathDraw>* m_pathDraws;
    bool m_allStrokes;
    bool m_allRoundJoin;
};

// Measure the speed of RiveRenderer::drawPath() from the contents of a .riv
// file.
class DrawRiveRenderPaths : public DrawRiveRenderPath
{
public:
    DrawRiveRenderPaths(bool allStrokes = false, bool allRoundJoin = false)
    {
        // Sniff paths out of a .riv file.
        SniffPathsRenderer sniffer(&m_pathDraws, allStrokes, allRoundJoin);
        std::unique_ptr<File> rivFile =
            File::import(assets::paper_riv(), m_nullContext.get());
        std::unique_ptr<ArtboardInstance> artboard = rivFile->artboardDefault();
        std::unique_ptr<Scene> scene = artboard->defaultScene();
        scene->advanceAndApply(0);
        scene->draw(&sniffer);
    }
};

REGISTER_BENCH(DrawRiveRenderPaths);

// Measure the speed of RiveRenderer::drawPath() from the contents of a .riv
// file, all paths converted to strokes.
class DrawRiveRenderPathsAsStrokes : public DrawRiveRenderPaths
{
public:
    DrawRiveRenderPathsAsStrokes() : DrawRiveRenderPaths(true) {}
};
REGISTER_BENCH(DrawRiveRenderPathsAsStrokes);

class DrawRiveRenderPathsAsRoundJoinStrokes : public DrawRiveRenderPaths
{
public:
    DrawRiveRenderPathsAsRoundJoinStrokes() : DrawRiveRenderPaths(true, true) {}
};
REGISTER_BENCH(DrawRiveRenderPathsAsRoundJoinStrokes);

class DrawCustomRiveRenderPaths : public DrawRiveRenderPath
{
public:
    void setup() final
    {
        for (int i = 0; i < 1000; ++i)
        {
            RawPath rawPath;
            buildPath(rawPath);
            auto renderPath = static_rcp_cast<RiveRenderPath>(
                m_nullContext->makeRenderPath(rawPath, FillRule::nonZero));
            auto renderPaint = static_rcp_cast<RiveRenderPaint>(
                m_nullContext->makeRenderPaint());
            renderPaint->style(RenderPaintStyle::stroke);
            renderPaint->thickness(2);
            m_pathDraws.emplace_back(std::move(renderPath),
                                     std::move(renderPaint));
        }
    }

protected:
    virtual void buildPath(RawPath& rawPath) = 0;
};

class DrawZeroChopStrokes : public DrawCustomRiveRenderPaths
{
protected:
    void buildPath(RawPath& rawPath) final
    {
        rawPath.moveTo(199, 1225);
        for (int j = 0; j < 50; ++j)
        {
            rawPath.cubicTo(197, 943, 349, 607, 549, 427);
            rawPath.cubicTo(349, 607, 197, 943, 199, 1225);
        }
    }
};

REGISTER_BENCH(DrawZeroChopStrokes);

class DrawOneChopStrokes : public DrawCustomRiveRenderPaths
{
protected:
    void buildPath(RawPath& rawPath) final
    {
        for (int j = 0; j < 50; ++j)
        {
            rawPath.cubicTo(100, 0, 50, 100, 100, 100);
            rawPath.cubicTo(0, -100, 200, 100, 0, 0);
        }
    }
};

REGISTER_BENCH(DrawOneChopStrokes);

class DrawTwoChopStrokes : public DrawCustomRiveRenderPaths
{
protected:
    void buildPath(RawPath& rawPath) final
    {
        rawPath.moveTo(460, 1060);
        for (int j = 0; j < 50; ++j)
        {
            rawPath.cubicTo(403, -320, 60, 660, 1181, 634);
            rawPath.cubicTo(60, 660, 403, -320, 460, 1060);
        }
    }
};

REGISTER_BENCH(DrawTwoChopStrokes);

class DrawOneCuspStrokes : public DrawCustomRiveRenderPaths
{
protected:
    void buildPath(RawPath& rawPath) final
    {
        for (int j = 0; j < 50; ++j)
        {
            rawPath.cubicTo(100, 100, 100, 0, 0, 100);
            rawPath.cubicTo(100, 0, 100, 100, 0, 0);
        }
    }
};

REGISTER_BENCH(DrawOneCuspStrokes);

class DrawTwoCuspStrokes : public DrawCustomRiveRenderPaths
{
protected:
    void buildPath(RawPath& rawPath) final
    {
        for (int j = 0; j < 50; ++j)
        {
            rawPath.cubicTo(100, 0, 50, 0, 150, 0);
            rawPath.cubicTo(50, 0, 100, 0, 0, 0);
        }
    }
};

REGISTER_BENCH(DrawTwoCuspStrokes);
