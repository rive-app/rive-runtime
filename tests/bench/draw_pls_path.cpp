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

using PathPaint = std::tuple<rcp<RiveRenderPath>, rcp<RiveRenderPaint>>;

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
            for (const auto& [path, paint] : m_pathPaints)
            {
                m_renderer.drawPath(path.get(), paint.get());
            }
            m_nullContext->flush({.renderTarget = m_renderTarget.get()});
        }
        return 0;
    }

    std::vector<PathPaint> m_pathPaints;
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
    SniffPathsRenderer(std::vector<PathPaint>* pathPaints,
                       bool allStrokes,
                       bool allRoundJoin,
                       float forcedFeather = 0) :
        m_pathPaints(pathPaints),
        m_allStrokes(allStrokes),
        m_allRoundJoin(allRoundJoin),
        m_forcedFeather(forcedFeather)
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
        if (m_forcedFeather)
        {
            renderPaint->feather(m_forcedFeather);
            renderPath->fillRule(FillRule::clockwise);
        }

        m_pathPaints->push_back({ref_rcp(renderPath), ref_rcp(renderPaint)});
    }
    void clipPath(RenderPath* path) override {}
    void drawImage(const RenderImage*, ImageSampler, BlendMode, float) override
    {}
    void drawImageMesh(const RenderImage*,
                       ImageSampler,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode,
                       float) override
    {}

private:
    std::vector<PathPaint>* m_pathPaints;
    bool m_allStrokes;
    bool m_allRoundJoin;
    float m_forcedFeather;
};

// Measure the speed of RiveRenderer::drawPath() from the contents of a .riv
// file.
class DrawRiveRenderPaths : public DrawRiveRenderPath
{
public:
    DrawRiveRenderPaths(bool allStrokes = false, bool allRoundJoin = false)
    {
        // Sniff paths out of a .riv file.
        SniffPathsRenderer sniffer(&m_pathPaints, allStrokes, allRoundJoin);
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

class DrawCustomPaths : public DrawRiveRenderPath
{
public:
    void setup() final
    {
        for (int i = 0; i < 1000; ++i)
        {
            RawPath rawPath;
            auto renderPath = static_rcp_cast<RiveRenderPath>(
                m_nullContext->makeRenderPath(rawPath, FillRule::clockwise));
            setupPath(renderPath.get());
            auto renderPaint = static_rcp_cast<RiveRenderPaint>(
                m_nullContext->makeRenderPaint());
            setupPaint(renderPaint.get());

            m_pathPaints.emplace_back(std::move(renderPath),
                                      std::move(renderPaint));
        }
    }

protected:
    virtual void setupPath(RiveRenderPath* path) = 0;
    virtual void setupPaint(RiveRenderPaint* paint) {}
};

class DrawCustomStrokes : public DrawCustomPaths
{
public:
    void setupPaint(RiveRenderPaint* paint) override
    {
        paint->style(RenderPaintStyle::stroke);
        paint->thickness(2);
    }
};

class DrawZeroChopStrokes : public DrawCustomStrokes
{
protected:
    void setupPath(RiveRenderPath* path) final
    {
        path->moveTo(199, 1225);
        for (int j = 0; j < 50; ++j)
        {
            path->cubicTo(197, 943, 349, 607, 549, 427);
            path->cubicTo(349, 607, 197, 943, 199, 1225);
        }
    }
};

REGISTER_BENCH(DrawZeroChopStrokes);

class DrawOneChopStrokes : public DrawCustomStrokes
{
protected:
    void setupPath(RiveRenderPath* path) final
    {
        for (int j = 0; j < 50; ++j)
        {
            path->cubicTo(100, 0, 50, 100, 100, 100);
            path->cubicTo(0, -100, 200, 100, 0, 0);
        }
    }
};

REGISTER_BENCH(DrawOneChopStrokes);

class DrawTwoChopStrokes : public DrawCustomStrokes
{
protected:
    void setupPath(RiveRenderPath* path) final
    {
        path->moveTo(460, 1060);
        for (int j = 0; j < 50; ++j)
        {
            path->cubicTo(403, -320, 60, 660, 1181, 634);
            path->cubicTo(60, 660, 403, -320, 460, 1060);
        }
    }
};

REGISTER_BENCH(DrawTwoChopStrokes);

class DrawOneCuspStrokes : public DrawCustomStrokes
{
protected:
    void setupPath(RiveRenderPath* path) final
    {
        for (int j = 0; j < 50; ++j)
        {
            path->cubicTo(100, 100, 100, 0, 0, 100);
            path->cubicTo(100, 0, 100, 100, 0, 0);
        }
    }
};

REGISTER_BENCH(DrawOneCuspStrokes);

class DrawTwoCuspStrokes : public DrawCustomStrokes
{
protected:
    void setupPath(RiveRenderPath* path) final
    {
        for (int j = 0; j < 50; ++j)
        {
            path->cubicTo(100, 0, 50, 0, 150, 0);
            path->cubicTo(50, 0, 100, 0, 0, 0);
        }
    }
};

class DrawFeatheredPaths : public DrawRiveRenderPath
{
public:
    DrawFeatheredPaths(rive::Span<uint8_t> riv)
    {
        // Sniff paths out of a .riv file.
        SniffPathsRenderer sniffer(&m_pathPaints,
                                   /*allStrokes=*/false,
                                   /*allRoundJoin=*/false,
                                   /*forcedFeather=*/100);
        std::unique_ptr<File> rivFile = File::import(riv, m_nullContext.get());
        std::unique_ptr<ArtboardInstance> artboard = rivFile->artboardDefault();
        std::unique_ptr<Scene> scene = artboard->defaultScene();
        scene->advanceAndApply(0);
        scene->draw(&sniffer);
    }
};

class DrawFeatheredPaths_paper : public DrawFeatheredPaths
{
public:
    DrawFeatheredPaths_paper() : DrawFeatheredPaths(assets::paper_riv()) {}
};
REGISTER_BENCH(DrawFeatheredPaths_paper);

class DrawCustomFeathers : public DrawCustomPaths
{
public:
    void setupPath(RiveRenderPath* path) final
    {
        path->fillRule(FillRule::clockwise);
        for (int j = 0; j < 50; ++j)
        {
            path->cubicTo(-800, 1600, 2400, 1600, 1600, 0);
        }
    }

    void setupPaint(RiveRenderPaint* paint) override
    {
        paint->style(RenderPaintStyle::fill);
        paint->feather(85);
    }
};
REGISTER_BENCH(DrawCustomFeathers);

REGISTER_BENCH(DrawTwoCuspStrokes);
