/*
 * Copyright 2026 Rive
 */

// A RenderPassRecording must emit exactly the stream a hand built
// OreCommandBuffer would, compared by silver. Null resources capture as
// kInvalidHandle so no GPU is required.

#include "rive/renderer/ore/cmd/ore_render_pass_recording.hpp"
#include "rive/renderer/ore/cmd/ore_command_silver.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_render_pass.hpp"

#include <algorithm>
#include <catch.hpp>
#include <memory>
#include <vector>

using namespace rive::ore;
using namespace rive::ore::cmd;

TEST_CASE("RenderPassRecording emits the expected command stream", "[ore][cmd]")
{
    RenderPassDesc desc;
    desc.colorCount = 1;
    desc.colorAttachments[0].view = nullptr;
    desc.colorAttachments[0].resolveTarget = nullptr;
    desc.colorAttachments[0].loadOp = LoadOp::clear;
    desc.colorAttachments[0].storeOp = StoreOp::store;
    desc.colorAttachments[0].clearColor = {0.25f, 0.5f, 0.75f, 1.0f};
    desc.depthStencil.view = nullptr;

    OreCommandBuffer recorded;
    {
        // A null context is safe, validation only dereferences it to report
        // errors.
        RenderPassRecording pass(nullptr, &recorded, desc);
        pass.setPipeline(nullptr);
        pass.setViewport(0.f, 0.f, 128.f, 64.f, 0.f, 1.f);
        pass.setScissorRect(0, 0, 128, 64);
        pass.draw(6, 1, 0, 0);
        pass.finish();
    }

    // Hand built reference stream.
    OreCommandBuffer expected;
    BeginRenderPassCmd begin{};
    begin.colorCount = 1;
    begin.colors[0] = {kInvalidHandle,
                       kInvalidHandle,
                       0.25f,
                       0.5f,
                       0.75f,
                       1.0f,
                       LoadOp::clear,
                       StoreOp::store};
    begin.depthStencil.view = kInvalidHandle;
    begin.depthStencil.depthLoadOp = LoadOp::clear;
    begin.depthStencil.depthStoreOp = StoreOp::store;
    begin.depthStencil.depthClearValue = 1.0f;
    begin.depthStencil.stencilLoadOp = LoadOp::clear;
    begin.depthStencil.stencilStoreOp = StoreOp::discard;
    begin.depthStencil.stencilClearValue = 0;
    expected.append(CommandType::beginRenderPass, begin);
    // A null pipeline records an invalid handle.
    expected.append(CommandType::setPipeline, SetPipelineCmd{kInvalidHandle});
    expected.append(CommandType::setViewport,
                    SetViewportCmd{0.f, 0.f, 128.f, 64.f, 0.f, 1.f});
    expected.append(CommandType::setScissorRect,
                    SetScissorRectCmd{0, 0, 128, 64});
    expected.append(CommandType::draw, DrawCmd{6, 1, 0, 0});
    expected.appendOpcode(CommandType::finish);

    std::vector<uint8_t> recordedSilver, expectedSilver;
    serializeSilver(recorded, recordedSilver);
    serializeSilver(expected, expectedSilver);
    CHECK(silverMatch(expectedSilver, recordedSilver));
}

TEST_CASE("RenderPassRecording finish is idempotent", "[ore][cmd]")
{
    RenderPassDesc desc;
    desc.colorCount = 0;
    desc.depthStencil.view = nullptr;

    OreCommandBuffer recorded;
    RenderPassRecording pass(nullptr, &recorded, desc);
    pass.draw(3, 1, 0, 0);
    pass.finish();
    CHECK(pass.isFinished());
    pass.finish();

    OreCommandReader r(recorded.commandBytes(), recorded.blobBytes());
    CommandType t;
    int finishes = 0;
    int total = 0;
    while (r.next(t))
    {
        ++total;
        if (t == CommandType::finish)
        {
            ++finishes;
            continue;
        }
        // The reader requires consuming each payload.
        switch (t)
        {
            case CommandType::beginRenderPass:
                r.read<BeginRenderPassCmd>();
                break;
            case CommandType::draw:
                r.read<DrawCmd>();
                break;
            default:
                FAIL("unexpected command in stream");
        }
    }
    CHECK(finishes == 1);
    CHECK(total == 3);
}

// ---------------------------------------------------------------------------
// Nested passes. A recorded pass never holds a live encoder, so a script may
// begin one inside another. The nested pass has to end up ahead of the pass it
// was begun inside, whole, with the rest of the stream in record order.
// ---------------------------------------------------------------------------

namespace
{
// One entry per command, draws and viewports carrying the value that names
// which pass issued them.
struct Op
{
    CommandType type;
    uint32_t value;
    bool operator==(const Op& o) const
    {
        return type == o.type && value == o.value;
    }
};

std::vector<Op> opsOf(const OreCommandBuffer& stream)
{
    std::vector<Op> ops;
    OreCommandReader reader(stream.commandBytes(), stream.blobBytes());
    CommandType type;
    while (reader.next(type))
    {
        uint32_t value = 0;
        switch (type)
        {
            case CommandType::draw:
                value = reader.read<DrawCmd>().vertexCount;
                break;
            case CommandType::setViewport:
                value = static_cast<uint32_t>(reader.read<SetViewportCmd>().x);
                break;
            case CommandType::bufferUpdate:
                value = reader.read<BufferUpdatePOD>().offset;
                break;
            default:
                reader.skip(orePayloadSizeOf(type));
                break;
        }
        ops.push_back({type, value});
    }
    return ops;
}

Op begin() { return {CommandType::beginRenderPass, 0}; }
Op draw(uint32_t n) { return {CommandType::draw, n}; }
Op finish() { return {CommandType::finish, 0}; }

// Stands in for a live backend: every pass it opens logs into `log` and the
// count of simultaneously open passes is what the encoder rule forbids
// exceeding.
class LiveStubContext : public Context
{
public:
    LiveStubContext() : Context(nullptr) {}

    std::vector<Op> log;
    int open = 0;
    int maxOpen = 0;
    bool frameReplay = false;

    class Pass : public RenderPass
    {
    public:
        Pass(LiveStubContext* ctx) : RenderPass(ctx), m_ctx(ctx)
        {
            m_ctx->log.push_back(begin());
            m_ctx->maxOpen = std::max(m_ctx->maxOpen, ++m_ctx->open);
        }
        ~Pass() override { finish(); }
        void setPipeline(Pipeline*) override {}
        void setVertexBuffer(uint32_t, Buffer*, uint32_t) override {}
        void setIndexBuffer(Buffer*, IndexFormat, uint32_t) override {}
        void setBindGroup(uint32_t,
                          BindGroup*,
                          const uint32_t*,
                          uint32_t) override
        {}
        void setViewport(float x, float, float, float, float, float) override
        {
            m_ctx->log.push_back(
                {CommandType::setViewport, static_cast<uint32_t>(x)});
        }
        void setScissorRect(uint32_t, uint32_t, uint32_t, uint32_t) override {}
        void setStencilReference(uint32_t) override {}
        void setBlendColor(float, float, float, float) override {}
        void draw(uint32_t n, uint32_t, uint32_t, uint32_t) override
        {
            m_ctx->log.push_back(::draw(n));
        }
        void drawIndexed(uint32_t,
                         uint32_t,
                         uint32_t,
                         int32_t,
                         uint32_t) override
        {}
        void finish() override
        {
            if (m_finished)
            {
                return;
            }
            m_finished = true;
            m_ctx->log.push_back(::finish());
            m_ctx->open--;
        }

    private:
        LiveStubContext* m_ctx;
    };

    std::unique_ptr<RenderPass> beginRenderPass(const RenderPassDesc&,
                                                std::string*) override
    {
        return std::make_unique<Pass>(this);
    }
    bool usesDeferredFrameReplay() const override { return frameReplay; }

    rive::rcp<Buffer> makeBuffer(const BufferDesc&) override { return nullptr; }
    rive::rcp<Texture> makeTexture(const TextureDesc&) override
    {
        return nullptr;
    }
    rive::rcp<TextureView> makeTextureViewImpl(const TextureViewDesc&) override
    {
        return nullptr;
    }
    rive::rcp<Sampler> makeSampler(const SamplerDesc&) override
    {
        return nullptr;
    }
    rive::rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc&) override
    {
        return nullptr;
    }
    rive::rcp<BindGroupLayout> makeBindGroupLayout(
        const BindGroupLayoutDesc&) override
    {
        return nullptr;
    }
    rive::rcp<Pipeline> makePipeline(const PipelineDesc&, std::string*) override
    {
        return nullptr;
    }
    rive::rcp<BindGroup> makeBindGroup(const BindGroupDesc&) override
    {
        return nullptr;
    }
    void beginFrame(const FrameDescriptor&) override {}
    void endFrame() override {}
    void waitForGPU() override {}
    rive::rcp<TextureView> wrapCanvasTexture(rive::gpu::RenderCanvas*) override
    {
        return nullptr;
    }
    rive::rcp<TextureView> wrapRiveTexture(rive::gpu::Texture*,
                                           uint32_t,
                                           uint32_t) override
    {
        return nullptr;
    }
    ShaderTarget shaderTarget() const override { return ShaderTarget::wgsl; }
};
} // namespace

TEST_CASE("a pass begun inside another lands ahead of it, whole",
          "[ore][cmd][nested]")
{
    DeferredOreContext ctx(nullptr);
    auto outer = ctx.beginRenderPass({});
    outer->draw(1);
    auto inner = ctx.beginRenderPass({});
    inner->draw(2);
    inner->finish();
    outer->draw(3);
    outer->finish();

    CHECK(opsOf(ctx.stream()) == std::vector<Op>{begin(),
                                                 draw(2),
                                                 finish(),
                                                 begin(),
                                                 draw(1),
                                                 draw(3),
                                                 finish()});
    CHECK_FALSE(ctx.hasOpenRenderPasses());
}

// Only the enclosing pass's own commands move past the nested pass. A
// resource made or written in between keeps its place, so a create still
// precedes every use and a write still lands before the passes after it.
TEST_CASE("lifecycle commands between the two begins keep their place",
          "[ore][cmd][nested]")
{
    DeferredOreContext ctx(nullptr);
    auto outer = ctx.beginRenderPass({});
    outer->draw(1);
    BufferDesc bd{};
    bd.size = 16;
    auto buffer = ctx.makeBuffer(bd);
    uint8_t bytes[4] = {};
    buffer->update(bytes, sizeof(bytes), 8);
    auto inner = ctx.beginRenderPass({});
    inner->draw(2);
    inner->finish();
    outer->finish();

    CHECK(opsOf(ctx.stream()) == std::vector<Op>{
                                     {CommandType::makeBuffer, 0},
                                     {CommandType::bufferUpdate, 8},
                                     begin(),
                                     draw(2),
                                     finish(),
                                     begin(),
                                     draw(1),
                                     finish(),
                                 });
}

// Deeper nesting settles from the inside out: each pass lands ahead of the one
// it was begun inside, and a pass already settled is never moved again.
TEST_CASE("three levels of nesting settle innermost first",
          "[ore][cmd][nested]")
{
    DeferredOreContext ctx(nullptr);
    auto a = ctx.beginRenderPass({});
    a->draw(1);
    auto b = ctx.beginRenderPass({});
    b->draw(2);
    auto c = ctx.beginRenderPass({});
    c->draw(3);
    c->finish();
    b->draw(22);
    b->finish();
    a->draw(11);
    a->finish();

    CHECK(opsOf(ctx.stream()) == std::vector<Op>{begin(),
                                                 draw(3),
                                                 finish(),
                                                 begin(),
                                                 draw(2),
                                                 draw(22),
                                                 finish(),
                                                 begin(),
                                                 draw(1),
                                                 draw(11),
                                                 finish()});
}

TEST_CASE("sibling nested passes keep their order ahead of the enclosing one",
          "[ore][cmd][nested]")
{
    DeferredOreContext ctx(nullptr);
    auto outer = ctx.beginRenderPass({});
    outer->draw(1);
    auto first = ctx.beginRenderPass({});
    first->draw(2);
    first->finish();
    outer->draw(11);
    auto second = ctx.beginRenderPass({});
    second->draw(3);
    second->finish();
    outer->draw(111);
    outer->finish();

    CHECK(opsOf(ctx.stream()) == std::vector<Op>{begin(),
                                                 draw(2),
                                                 finish(),
                                                 begin(),
                                                 draw(3),
                                                 finish(),
                                                 begin(),
                                                 draw(1),
                                                 draw(11),
                                                 draw(111),
                                                 finish()});
}

// A script may finish the enclosing pass with the nested one still open. The
// nested pass is finished for it, so the stream stays well formed and its
// wrapper sees a finished pass instead of recording into a closed range.
TEST_CASE("finishing the enclosing pass finishes the nested one first",
          "[ore][cmd][nested]")
{
    DeferredOreContext ctx(nullptr);
    auto outer = ctx.beginRenderPass({});
    outer->draw(1);
    auto inner = ctx.beginRenderPass({});
    inner->draw(2);
    outer->finish();

    CHECK(inner->isFinished());
    CHECK_FALSE(ctx.hasOpenRenderPasses());
    CHECK(opsOf(ctx.stream()) == std::vector<Op>{begin(),
                                                 draw(2),
                                                 finish(),
                                                 begin(),
                                                 draw(1),
                                                 finish()});
}

TEST_CASE("a destroyed unfinished pass finishes and settles",
          "[ore][cmd][nested]")
{
    DeferredOreContext ctx(nullptr);
    auto outer = ctx.beginRenderPass({});
    outer->draw(1);
    {
        auto inner = ctx.beginRenderPass({});
        inner->draw(2);
    }
    outer->finish();

    CHECK(opsOf(ctx.stream()) == std::vector<Op>{begin(),
                                                 draw(2),
                                                 finish(),
                                                 begin(),
                                                 draw(1),
                                                 finish()});
}

// The enclosing pass's own commands settle after the nested pass, so a write
// it made after binding a buffer reaches its earlier draws too, the way a
// whole WebGPU pass sees a queue write. Pinning those draws to the old
// contents would take versioned binds.
TEST_CASE("a write during the enclosing pass reaches its earlier draws",
          "[ore][cmd][nested]")
{
    DeferredOreContext ctx(nullptr);
    BufferDesc bd{};
    bd.size = 16;
    auto buffer = ctx.makeBuffer(bd);
    auto outer = ctx.beginRenderPass({});
    outer->setVertexBuffer(0, buffer.get());
    outer->draw(1);
    uint8_t bytes[4] = {};
    buffer->update(bytes, sizeof(bytes), 8);
    auto inner = ctx.beginRenderPass({});
    inner->draw(2);
    inner->finish();
    outer->draw(3);
    outer->finish();

    CHECK(opsOf(ctx.stream()) == std::vector<Op>{
                                     {CommandType::makeBuffer, 0},
                                     {CommandType::bufferUpdate, 8},
                                     begin(),
                                     draw(2),
                                     finish(),
                                     begin(),
                                     {CommandType::setVertexBuffer, 0},
                                     draw(1),
                                     draw(3),
                                     finish(),
                                 });
}

// A script wrapper can be collected after the context it recorded into is
// gone. With nothing left to record into, the pass reads as finished and its
// finish and destructor stay quiet.
TEST_CASE("a pass outliving its context finishes quietly", "[ore][cmd][nested]")
{
    std::unique_ptr<RenderPass> outer, inner;
    {
        DeferredOreContext ctx(nullptr);
        outer = ctx.beginRenderPass({});
        outer->draw(1);
        inner = ctx.beginRenderPass({});
        inner->draw(2);
    }
    CHECK(outer->isFinished());
    CHECK(inner->isFinished());
    outer->finish();
    inner.reset();
    outer.reset();

    std::unique_ptr<RenderPass> inlinePass;
    {
        LiveStubContext ctx;
        inlinePass = beginRecordedRenderPass(ctx, {});
        inlinePass->draw(1);
    }
    CHECK(inlinePass->isFinished());
    inlinePass->finish();
    inlinePass.reset();
}

// What a script call's exit uses: it reclaims the passes begun after the
// token it entered with, innermost first, and nothing older.
TEST_CASE("reclaiming from a token finishes only the passes begun after it",
          "[ore][cmd][nested]")
{
    DeferredOreContext ctx(nullptr);
    auto outer = ctx.beginRenderPass({});
    uint64_t token = ctx.nextRenderPassToken();
    auto first = ctx.beginRenderPass({});
    auto second = ctx.beginRenderPass({});

    CHECK(ctx.finishOpenRenderPassesFrom(token) == 2);
    CHECK(first->isFinished());
    CHECK(second->isFinished());
    CHECK_FALSE(outer->isFinished());
    CHECK(ctx.hasOpenRenderPasses());

    CHECK(ctx.finishOpenRenderPassesFrom(token) == 0);
    CHECK(ctx.finishOpenRenderPassesFrom(0) == 1);
    CHECK(outer->isFinished());
    CHECK_FALSE(ctx.hasOpenRenderPasses());
}

// A frame reset with a pass still open would otherwise leave its finish to
// land at the head of the next frame's stream.
TEST_CASE("a frame reset finishes what a script left open",
          "[ore][cmd][nested]")
{
    DeferredOreContext ctx(nullptr);
    auto pass = ctx.beginRenderPass({});
    pass->draw(1);
    ctx.resetFrame();
    CHECK(pass->isFinished());
    CHECK(ctx.stream().empty());
    pass->finish();
    CHECK(ctx.stream().empty());
}

// On a live backend the script facing pass replays inline at finish, so a
// nested pass drains while the enclosing one is still only recording: the
// backend sees the nested pass first and never two encoders at once.
TEST_CASE("inline passes drain nested first without overlapping",
          "[ore][cmd][nested]")
{
    LiveStubContext ctx;
    auto outer = beginRecordedRenderPass(ctx, {});
    outer->draw(1);
    auto inner = beginRecordedRenderPass(ctx, {});
    inner->draw(2);
    inner->finish();
    CHECK(ctx.log == std::vector<Op>{begin(), draw(2), finish()});
    outer->draw(3);
    outer->finish();

    CHECK(ctx.log == std::vector<Op>{begin(),
                                     draw(2),
                                     finish(),
                                     begin(),
                                     draw(1),
                                     draw(3),
                                     finish()});
    CHECK(ctx.maxOpen == 1);
    CHECK_FALSE(ctx.hasOpenRenderPasses());
}

TEST_CASE("an inline pass finished by its enclosing pass still drains",
          "[ore][cmd][nested]")
{
    LiveStubContext ctx;
    auto outer = beginRecordedRenderPass(ctx, {});
    outer->draw(1);
    auto inner = beginRecordedRenderPass(ctx, {});
    inner->draw(2);
    outer->finish();

    CHECK(inner->isFinished());
    CHECK(ctx.log == std::vector<Op>{begin(),
                                     draw(2),
                                     finish(),
                                     begin(),
                                     draw(1),
                                     finish()});
    CHECK(ctx.maxOpen == 1);
}

// A backend that drains a whole frame at once records every script pass into
// pendingFrame, where nesting settles the same way as in a session stream.
TEST_CASE("frame replay backends settle nested passes in pendingFrame",
          "[ore][cmd][nested]")
{
    LiveStubContext ctx;
    ctx.frameReplay = true;
    ctx.setDeferredRecording(true);
    auto outer = beginRecordedRenderPass(ctx, {});
    outer->draw(1);
    auto inner = beginRecordedRenderPass(ctx, {});
    inner->draw(2);
    inner->finish();
    outer->draw(3);
    outer->finish();
    CHECK(ctx.log.empty());

    replayCommandBuffer(ctx, ctx.pendingFrame());
    CHECK(ctx.log == std::vector<Op>{begin(),
                                     draw(2),
                                     finish(),
                                     begin(),
                                     draw(1),
                                     draw(3),
                                     finish()});
    CHECK(ctx.maxOpen == 1);
}
