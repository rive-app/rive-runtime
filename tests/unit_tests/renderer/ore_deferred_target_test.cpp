/*
 * Copyright 2026 Rive
 */

// The host's render target reaches a recording only as plain values; the real
// target is wrapped at replay. These pin down what the recording writes for it
// and when the replayer keeps Rive from clearing over it. GPU free.

#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "deferred_test_sink.hpp"

#include <catch.hpp>
#include <memory>
#include <vector>

using namespace rive;
using namespace rive::ore;
using namespace rive::ore::cmd;

namespace
{
Context::TargetDesc target(uint32_t width,
                           uint32_t height,
                           TextureFormat format = TextureFormat::rgba8unorm,
                           uint32_t sampleCount = 1)
{
    return {width, height, format, sampleCount};
}

// The target wraps this frame's stream records, in order.
std::vector<WrapCanvasViewPOD> targetWraps(const DeferredOreContext& ctx)
{
    std::vector<WrapCanvasViewPOD> wraps;
    OreCommandReader reader(ctx.stream().commandBytes(),
                            ctx.stream().blobBytes());
    CommandType type;
    while (reader.next(type))
    {
        switch (type)
        {
            case CommandType::wrapCanvasView:
            {
                auto pod = reader.read<WrapCanvasViewPOD>();
                if (pod.mode ==
                    static_cast<uint32_t>(WrapCanvasViewMode::targetView))
                {
                    wraps.push_back(pod);
                }
                break;
            }
            case CommandType::beginRenderPass:
                reader.read<BeginRenderPassCmd>();
                break;
            case CommandType::finish:
                break;
            case CommandType::destroyResource:
                reader.read<DestroyResourcePOD>();
                break;
            default:
                FAIL("unexpected command in a target only stream");
        }
    }
    return wraps;
}

bool drawInto(Context& ctx, TextureView* view)
{
    RenderPassDesc rp{};
    rp.colorAttachments[0].view = view;
    rp.colorAttachments[0].loadOp = LoadOp::clear;
    rp.colorAttachments[0].storeOp = StoreOp::store;
    rp.colorCount = 1;
    auto pass = ctx.beginRenderPass(rp);
    if (pass == nullptr)
    {
        return false;
    }
    pass->finish();
    return true;
}

ResourceHandle idOf(const rcp<TextureView>& view)
{
    return static_cast<DeferredTextureView*>(view.get())->clientHandle();
}

class SizedTarget : public gpu::RenderTarget
{
public:
    SizedTarget(uint32_t width, uint32_t height) :
        gpu::RenderTarget(width, height)
    {}
};

// Records what the replayer tells the host about its target each frame.
class PreserveSink : public deferred_test::TestSink
{
public:
    std::vector<bool> preserved;
    bool hasOre = true;
    uint32_t screenWidth = 64;
    void setTargetPreserved(bool value) override { preserved.push_back(value); }
    Context* oreContext() override { return hasOre ? &m_ore : nullptr; }
    gpu::RenderTarget* targetRenderTarget() override
    {
        m_screen = std::make_unique<SizedTarget>(screenWidth, 64);
        return m_screen.get();
    }

private:
    DeferredOreContext m_ore{nullptr};
    std::unique_ptr<SizedTarget> m_screen;
};
} // namespace

TEST_CASE("a target view claims its replay slot when it is made",
          "[ore][cmd][target]")
{
    DeferredOreContext ctx(nullptr);
    ctx.setTarget(target(64, 32));

    auto view = ctx.targetView();
    REQUIRE(view != nullptr);
    CHECK(view->width() == 64u);
    CHECK(view->height() == 32u);

    auto wraps = targetWraps(ctx);
    REQUIRE(wraps.size() == 1);
    CHECK(wraps[0].id == idOf(view));

    // Asking again hands back the same view without claiming another slot.
    CHECK(ctx.targetView() == view);
    CHECK(targetWraps(ctx).size() == 1);
}

TEST_CASE("a frame wraps the target once however many passes draw into it",
          "[ore][cmd][target]")
{
    DeferredOreContext ctx(nullptr);
    ctx.setTarget(target(64, 64));
    auto view = ctx.targetView();

    // The wrap made with the view serves the frame it was made in.
    for (int i = 0; i < 3; ++i)
    {
        REQUIRE(drawInto(ctx, view.get()));
    }
    CHECK(targetWraps(ctx).size() == 1);
    CHECK(ctx.targetDrawn());

    ctx.resetFrame();
    for (int i = 0; i < 3; ++i)
    {
        REQUIRE(drawInto(ctx, view.get()));
    }
    auto wraps = targetWraps(ctx);
    REQUIRE(wraps.size() == 1);
    CHECK(wraps[0].id == idOf(view));
    CHECK(ctx.targetDrawn());

    // A frame that leaves the target alone neither wraps nor draws it.
    ctx.resetFrame();
    CHECK(ctx.targetView() == view);
    CHECK(targetWraps(ctx).empty());
    CHECK_FALSE(ctx.targetDrawn());
}

TEST_CASE("a target changing size or format mints a new view",
          "[ore][cmd][target]")
{
    DeferredOreContext ctx(nullptr);
    ctx.setTarget(target(64, 64));
    auto first = ctx.targetView();

    ctx.setTarget(target(64, 64));
    CHECK(ctx.targetView() == first);

    ctx.setTarget(target(128, 64));
    auto resized = ctx.targetView();
    REQUIRE(resized != nullptr);
    CHECK(resized != first);
    CHECK(resized->width() == 128u);

    ctx.setTarget(target(128, 64, TextureFormat::bgra8unorm, 4));
    auto reformatted = ctx.targetView();
    REQUIRE(reformatted != nullptr);
    CHECK(reformatted != resized);
    CHECK(reformatted->texture()->format() == TextureFormat::bgra8unorm);
    CHECK(reformatted->texture()->sampleCount() == 4u);
}

TEST_CASE("with no target declared there is no view and passes drop",
          "[ore][cmd][target]")
{
    DeferredOreContext ctx(nullptr);
    CHECK(ctx.targetView() == nullptr);

    ctx.setTarget(target(64, 64));
    auto stale = ctx.targetView();
    REQUIRE(stale != nullptr);
    ctx.resetFrame();

    ctx.setTarget({});
    CHECK(ctx.targetView() == nullptr);
    // A view kept from an earlier frame names an image that may be gone.
    CHECK_FALSE(drawInto(ctx, stale.get()));
    CHECK(targetWraps(ctx).empty());
    CHECK_FALSE(ctx.targetDrawn());

    // The same target coming back keeps its view.
    ctx.setTarget(target(64, 64));
    CHECK(ctx.targetView() == stale);
    CHECK(drawInto(ctx, stale.get()));

    // Replaced by a new target, the old view is refused.
    ctx.resetFrame();
    ctx.setTarget(target(128, 64));
    REQUIRE(ctx.targetView() != nullptr);
    CHECK_FALSE(drawInto(ctx, stale.get()));
    CHECK_FALSE(ctx.lastError().empty());
}

TEST_CASE("replay keeps the target only in frames a script drew into it",
          "[ore][cmd][target]")
{
    rive::cmd::DeferredSession session(ReplayCaps{});
    auto& ctx = session.oreContext();
    ctx.setTarget(target(64, 64));
    auto view = ctx.targetView();

    PreserveSink sink;
    rive::cmd::DeferredReplayer replayer;

    REQUIRE(drawInto(ctx, view.get()));
    replayer.replayFrame(rive::cmd::snapshotFrame(session), sink);

    session.resetFrame();
    replayer.replayFrame(rive::cmd::snapshotFrame(session), sink);

    session.resetFrame();
    REQUIRE(drawInto(ctx, view.get()));
    replayer.replayFrame(rive::cmd::snapshotFrame(session), sink);

    CHECK(sink.preserved == std::vector<bool>{true, false, true});
}

TEST_CASE("replay clears the target when no Ore context can draw it",
          "[ore][cmd][target]")
{
    rive::cmd::DeferredSession session(ReplayCaps{});
    auto& ctx = session.oreContext();
    ctx.setTarget(target(64, 64));
    auto view = ctx.targetView();

    PreserveSink sink;
    sink.hasOre = false;
    rive::cmd::DeferredReplayer replayer;

    REQUIRE(drawInto(ctx, view.get()));
    replayer.replayFrame(rive::cmd::snapshotFrame(session), sink);

    CHECK(sink.preserved == std::vector<bool>{false});
}

TEST_CASE("replay clears a target resized since the script drew into it",
          "[ore][cmd][target]")
{
    rive::cmd::DeferredSession session(ReplayCaps{});
    auto& ctx = session.oreContext();
    ctx.setTarget(target(64, 64));
    auto view = ctx.targetView();
    REQUIRE(drawInto(ctx, view.get()));
    rive::cmd::DeferredFrame frame = rive::cmd::snapshotFrame(session);

    PreserveSink sink;
    sink.screenWidth = 128;
    rive::cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);

    CHECK(sink.preserved == std::vector<bool>{false});
}

TEST_CASE("the target view cannot be sampled", "[ore][cmd][target]")
{
    DeferredOreContext ctx(nullptr);
    ctx.setTarget(target(64, 64));
    auto view = ctx.targetView();
    REQUIRE(view != nullptr);

    BindGroupDesc bg{};
    BindGroupDesc::TexEntry tex{};
    tex.view = view.get();
    bg.textures = &tex;
    bg.textureCount = 1;
    CHECK(ctx.makeBindGroup(bg) == nullptr);
    CHECK_FALSE(ctx.lastError().empty());
}
