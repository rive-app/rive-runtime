/*
 * Copyright 2026 Rive
 */

// What a script reads out of context.features while its GPU work is being
// recorded. The answer has to describe the device that will replay the stream,
// because a capability branch taken at record time is written into the stream
// and then replayed verbatim. An answer that is merely plausible is the worst
// case: replay runs the wrong branch flawlessly on hardware that contradicts
// it, and nothing downstream can tell.
//
// Driven through the construction factory's ore(), the same derivation every
// host that records uses by importing through a DeferredSession (the goldens
// RIVLoader, the player, the editor). The Flutter runtime on this branch never
// binds a render context, so it is not one of the reachable paths.

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "utils/no_op_factory.hpp"

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/ore_context.hpp"

#include <string>

using namespace rive;

namespace
{
// GPU free stand-in for a real backend context. Only what it advertises
// matters here, so the factories are unreachable.
class FakeDeviceContext : public ore::Context
{
public:
    FakeDeviceContext() : ore::Context(nullptr) {}

    ore::Features& editableFeatures() { return m_features; }

    rcp<ore::Buffer> makeBuffer(const ore::BufferDesc&) override
    {
        return nullptr;
    }
    rcp<ore::Texture> makeTexture(const ore::TextureDesc&) override
    {
        return nullptr;
    }
    rcp<ore::TextureView> makeTextureView(const ore::TextureViewDesc&) override
    {
        return nullptr;
    }
    rcp<ore::Sampler> makeSampler(const ore::SamplerDesc&) override
    {
        return nullptr;
    }
    rcp<ore::ShaderModule> makeShaderModule(
        const ore::ShaderModuleDesc&) override
    {
        return nullptr;
    }
    rcp<ore::BindGroupLayout> makeBindGroupLayout(
        const ore::BindGroupLayoutDesc&) override
    {
        return nullptr;
    }
    rcp<ore::Pipeline> makePipeline(const ore::PipelineDesc&,
                                    std::string*) override
    {
        return nullptr;
    }
    rcp<ore::BindGroup> makeBindGroup(const ore::BindGroupDesc&) override
    {
        return nullptr;
    }
    std::unique_ptr<ore::RenderPass> beginRenderPass(const ore::RenderPassDesc&,
                                                     std::string*) override
    {
        return nullptr;
    }
    void beginFrame(const FrameDescriptor&) override {}
    void endFrame() override {}
    void waitForGPU() override {}
    rcp<ore::TextureView> wrapCanvasTexture(gpu::RenderCanvas*) override
    {
        return nullptr;
    }
    rcp<ore::TextureView> wrapRiveTexture(gpu::Texture*,
                                          uint32_t,
                                          uint32_t) override
    {
        return nullptr;
    }
    ore::ShaderTarget shaderTarget() const override
    {
        return ore::ShaderTarget::glsl;
    }
};

// context.features reaches this through a namecall on a ScriptedContext bound
// to a ScriptedObject. Calling it as a bare global is the same entry point
// without that scaffolding.
int pushGPUFeatures(lua_State* L) { return lua_push_gpu_features(L); }

// Import factory shaped like a recording session: its ore() is the recorder.
class RecordingOreFactory : public NoOpFactory
{
public:
    ore::Context* recorder = nullptr;
    ore::Context* ore() override { return recorder; }
};

// Runs `source` against a VM whose ore context is `ore`, with the features
// readout exposed as a global. Returns the error message, empty on success.
std::string runWithOreContext(ore::Context* ore, const char* source)
{
    RecordingOreFactory factory;
    factory.recorder = ore;
    // Called here rather than through execute(), which only prints the
    // message this asserts on.
    ScriptingTest test(source, 0, true, {}, false, &factory);
    lua_State* L = test.state();
    lua_pushcfunction(L, pushGPUFeatures, "features");
    lua_setglobal(L, "gpuFeatures");
    if (lua_pcall(L, 0, 0, 0) == LUA_OK)
    {
        return {};
    }
    const char* message = lua_tostring(L, -1);
    std::string error = message != nullptr ? message : "unknown error";
    lua_pop(L, 1);
    return error;
}
} // namespace

TEST_CASE("a recording script reads the replay device's capabilities",
          "[scripting][gpu][features]")
{
    FakeDeviceContext device;
    ore::Features& real = device.editableFeatures();
    // A device more capable than Features' initializers in both directions: a
    // flag they deny and a limit they understate.
    real.colorBufferHalfFloat = true;
    real.maxSamples = 8;

    SECTION("bound at construction, as every native host binds")
    {
        ore::cmd::DeferredOreContext recorder(&device);
        std::string error = runWithOreContext(
            &recorder,
            "local f = gpuFeatures()\n"
            "assert(f.colorBufferHalfFloat == true, 'half float denied')\n"
            "assert(f.maxSamples == 8, 'maxSamples ' .. f.maxSamples)\n");
        CHECK(error.empty());
    }

    SECTION("bound late, as web binds on attach")
    {
        // bindReal used to store the pointer and copy nothing, so a script
        // running after attach still read the initializers.
        ore::cmd::DeferredOreContext recorder(nullptr);
        recorder.bindReal(&device);
        std::string error = runWithOreContext(
            &recorder,
            "local f = gpuFeatures()\n"
            "assert(f.colorBufferHalfFloat == true, 'half float denied')\n"
            "assert(f.maxSamples == 8, 'maxSamples ' .. f.maxSamples)\n");
        CHECK(error.empty());
    }

    SECTION("a real context answers as it always did")
    {
        std::string error = runWithOreContext(
            &device,
            "local f = gpuFeatures()\n"
            "assert(f.maxSamples == 8, 'maxSamples wrong')\n");
        CHECK(error.empty());
    }
}

TEST_CASE("an unbound recording context refuses to report capabilities",
          "[scripting][gpu][features]")
{
    // Refusing rather than answering conservatively, because the conservative
    // answer is what produces the silent wrong branch: it is indistinguishable
    // from a real low end device, so no script can defend itself against it,
    // and the branch it picks is baked into a stream that replays elsewhere.
    ore::cmd::DeferredOreContext recorder(nullptr);
    std::string error = runWithOreContext(&recorder, "local f = gpuFeatures()");
    CHECK(error.find("context.features") != std::string::npos);
}

TEST_CASE("an undecidable capability gate does not invent a refusal",
          "[scripting][gpu][features]")
{
    // The gates in lua_gpu.cpp are diagnostics; the real backend is the
    // authority. Gating on the initializers while unbound would reject a
    // format most devices render, which is the same fiction pointed the other
    // way -- and unlike a wrong readout it breaks content outright.
    const char* kScript = "local t = GPUTexture.new({ width = 4, height = 4, "
                          "format = 'rgba16float', renderTarget = true })";

    SECTION("unbound, the float renderTarget gate stays out of it")
    {
        ore::cmd::DeferredOreContext recorder(nullptr);
        std::string error = runWithOreContext(&recorder, kScript);
        CHECK(error.find("colorBufferHalfFloat") == std::string::npos);
    }

    SECTION("bound to a half float device, the gate does not fire")
    {
        FakeDeviceContext device;
        device.editableFeatures().colorBufferHalfFloat = true;
        ore::cmd::DeferredOreContext recorder(&device);
        std::string error = runWithOreContext(&recorder, kScript);
        CHECK(error.find("colorBufferHalfFloat") == std::string::npos);
    }

    SECTION("bound to a device without it, the gate still fires")
    {
        FakeDeviceContext device;
        ore::cmd::DeferredOreContext recorder(&device);
        std::string error = runWithOreContext(&recorder, kScript);
        CHECK(error.find("colorBufferHalfFloat") != std::string::npos);
    }
}
#endif
