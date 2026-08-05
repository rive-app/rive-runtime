/*
 * Copyright 2026 Rive
 */

// What a recording context answers about the device its stream will run on.
// A recorded capability branch is not a readout, it is a prediction that gets
// written into a stream and replayed on real hardware, so an answer that is
// merely plausible is worse than no answer at all: replay executes the wrong
// branch flawlessly and nothing downstream can tell.

#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/ore_context.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::ore;
using namespace rive::ore::cmd;

namespace
{
// GPU free stand-in for a real backend context: the only thing under test is
// what it advertises, so the factories are unreachable.
class FakeDeviceContext : public Context
{
public:
    FakeDeviceContext() : Context(nullptr) {}

    Features& editableFeatures() { return m_features; }

    rcp<Buffer> makeBuffer(const BufferDesc&) override { return nullptr; }
    rcp<Texture> makeTexture(const TextureDesc&) override { return nullptr; }
    rcp<TextureView> makeTextureView(const TextureViewDesc&) override
    {
        return nullptr;
    }
    rcp<Sampler> makeSampler(const SamplerDesc&) override { return nullptr; }
    rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc&) override
    {
        return nullptr;
    }
    rcp<BindGroupLayout> makeBindGroupLayout(
        const BindGroupLayoutDesc&) override
    {
        return nullptr;
    }
    rcp<Pipeline> makePipeline(const PipelineDesc&, std::string*) override
    {
        return nullptr;
    }
    rcp<BindGroup> makeBindGroup(const BindGroupDesc&) override
    {
        return nullptr;
    }
    std::unique_ptr<RenderPass> beginRenderPass(const RenderPassDesc&,
                                                std::string*) override
    {
        return nullptr;
    }
    void beginFrame(const FrameDescriptor&) override {}
    void endFrame() override {}
    void waitForGPU() override {}
    rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas*) override
    {
        return nullptr;
    }
    rcp<TextureView> wrapRiveTexture(gpu::Texture*, uint32_t, uint32_t) override
    {
        return nullptr;
    }
    ShaderTarget shaderTarget() const override { return ShaderTarget::glsl; }
};
} // namespace

TEST_CASE("a recording context reports the replay device's capabilities",
          "[ore][cmd][deferred]")
{
    FakeDeviceContext device;
    Features& real = device.editableFeatures();
    // A device more capable than Features' initializers in both directions:
    // a flag they deny and a limit they understate.
    real.colorBufferHalfFloat = true;
    real.maxSamples = 8;
    real.maxTextureSize2D = 16384;

    SECTION("bound at construction, as every native host binds")
    {
        DeferredOreContext recorder(&device);
        CHECK(recorder.featuresKnown());
        CHECK(recorder.features().colorBufferHalfFloat);
        CHECK(recorder.features().maxSamples == 8u);
        CHECK(recorder.features().maxTextureSize2D == 16384u);
    }

    SECTION("bound late, as web binds on attach")
    {
        DeferredOreContext recorder(nullptr);
        recorder.bindReal(&device);
        CHECK(recorder.featuresKnown());
        CHECK(recorder.features().colorBufferHalfFloat);
        CHECK(recorder.features().maxSamples == 8u);
    }

    SECTION("unbound, nothing has been measured and it says so")
    {
        // The values are still Features' initializers, which is exactly why
        // featuresKnown has to exist: they read as a real, poor device, so no
        // caller can distinguish a guess from a measurement by inspecting them.
        DeferredOreContext recorder(nullptr);
        CHECK_FALSE(recorder.featuresKnown());
        CHECK_FALSE(recorder.features().colorBufferHalfFloat);
    }
}

TEST_CASE("a real context always knows its own capabilities",
          "[ore][cmd][deferred]")
{
    FakeDeviceContext device;
    CHECK(device.featuresKnown());
}
