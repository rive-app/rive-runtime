/*
 * Copyright 2026 Rive
 *
 * WebGPU lets a pipeline take its vertex and fragment from different shader
 * modules. Ore accepted the shape but read both stages off the vertex
 * module's binding map, so the fragment's own resources were absent from the
 * layout and could not be bound at all.
 */

#include "common/testing_window.hpp"
#include "gm/ore_gm_helper.hpp"
#include <catch.hpp>

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_VK) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_D3D12) ||                 \
    defined(ORE_BACKEND_D3D11) || defined(ORE_BACKEND_GL)
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include <cstdlib>
#ifdef __APPLE__
#include <dlfcn.h>
#endif
#define ORE_SPLIT_STAGE_TEST_ACTIVE
#endif

#ifdef ORE_SPLIT_STAGE_TEST_ACTIVE
using namespace rive;
using namespace rive::ore;

namespace
{
// GL and D3D11 matter most here — their slot namespaces reach furthest, so
// they are where a split-stage pipeline can bind the wrong resource.
// Apple's GL is ANGLE, which a plain checkout has not built.
const TestingWindow::Backend kOreBackends[] = {
#if defined(ORE_BACKEND_METAL)
    TestingWindow::Backend::metal,
#endif
#if defined(ORE_BACKEND_D3D11)
    TestingWindow::Backend::d3d,
#endif
#if defined(ORE_BACKEND_D3D12)
    TestingWindow::Backend::d3d12,
#endif
#if defined(ORE_BACKEND_VK)
    TestingWindow::Backend::vk,
#endif
#if defined(ORE_BACKEND_GL)
#if defined(__APPLE__)
    // GL here is ANGLE, whose EGL pbuffer needs no display server — the one
    // GL flavor a CI runner can start.
    TestingWindow::Backend::angle,
#else
    TestingWindow::Backend::gl,
#endif
#endif
#if defined(ORE_BACKEND_WGPU)
    TestingWindow::Backend::dawn,
#endif
};

// `TestingWindow::Init` aborts on a backend it cannot bring up rather than
// declining it, which would take every test in the binary with it. So check
// what each GL flavor needs before asking for it.
//
// The EGL path only speaks ANGLE: it dlsyms `eglGetPlatformDisplayEXT`,
// which Mesa's libEGL does not export, and Android trips an
// `angleRenderer == EGL_NONE` assert. Hence ANGLE on Apple, a real GLFW
// window everywhere else.
bool backendCanStart(TestingWindow::Backend backend)
{
    if (backend == TestingWindow::Backend::gl)
    {
        return getenv("DISPLAY") != nullptr ||
               getenv("WAYLAND_DISPLAY") != nullptr;
    }
#ifdef __APPLE__
    if (backend == TestingWindow::Backend::angle)
    {
        // Held for the process; MakeEGL dlopens it again anyway.
        return dlopen("libEGL.dylib", RTLD_LAZY) != nullptr;
    }
#endif
    return true;
}

struct Uniforms
{
    float r, g, b, a;
};

void runScenario(TestingWindow* window)
{
    auto* renderContext = window->renderContext();
    ore_gm::OreGMContext oreGM;
    REQUIRE(oreGM.ensureContext(renderContext));
    auto& ctx = *renderContext->getOreContext();

    // Two files: a vertex that declares no resources, and a fragment whose
    // two UBOs exist only in its own map.
    auto vertexShader = ore_gm::loadShader(ctx, ore_gm::kTriangle);
    auto fragmentShader = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
    REQUIRE(vertexShader.vsModule);
    REQUIRE(fragmentShader.psModule);
    REQUIRE(vertexShader.vsModule.get() != fragmentShader.psModule.get());

    // What the old path produced: the vertex module knows about none of the
    // fragment's bindings, so its layout for group 0 is empty.
    auto vertexOnly =
        makeBindGroupLayoutFromShader(ctx, vertexShader.vsModule.get(), 0);
    REQUIRE(vertexOnly != nullptr);
    CHECK(vertexOnly->entries().empty());

    const BindingMap merged =
        bindingMapForStages(vertexShader.vsModule.get(),
                            fragmentShader.psModule.get());
    auto layout = makeBindGroupLayoutFromBindingMap(ctx, merged, 0);
    REQUIRE(layout != nullptr);
    REQUIRE(layout->entries().size() == 2);
    for (const BindGroupLayoutEntry& entry : layout->entries())
    {
        CHECK((entry.visibility.mask & StageVisibility::kFragment) != 0);
        // Slots resolved from the module that emitted the fragment source.
        CHECK(entry.nativeSlotFS != BindGroupLayoutEntry::kNativeSlotAbsent);
    }

    BindGroupLayout* layouts[] = {layout.get()};
    PipelineDesc pipeDesc{};
    pipeDesc.vertexModule = vertexShader.vsModule.get();
    pipeDesc.fragmentModule = fragmentShader.psModule.get();
    pipeDesc.vertexEntryPoint = vertexShader.vsEntryPoint;
    pipeDesc.fragmentEntryPoint = fragmentShader.fsEntryPoint;
    VertexAttribute attrs[2] = {
        {offsetof(ore_gm::TriVertex, x), 0, VertexFormat::float2},
        {offsetof(ore_gm::TriVertex, r), 1, VertexFormat::float4},
    };
    VertexBufferLayout vertexLayout{};
    vertexLayout.stride = sizeof(ore_gm::TriVertex);
    vertexLayout.stepMode = VertexStepMode::vertex;
    vertexLayout.attributes = attrs;
    vertexLayout.attributeCount = 2;
    pipeDesc.vertexBuffers = &vertexLayout;
    pipeDesc.vertexBufferCount = 1;
    pipeDesc.topology = PrimitiveTopology::triangleList;
    pipeDesc.colorTargets[0].format = TextureFormat::rgba8unorm;
    pipeDesc.colorCount = 1;
    pipeDesc.depthStencil.depthCompare = CompareFunction::always;
    pipeDesc.depthStencil.depthWriteEnabled = false;
    pipeDesc.bindGroupLayouts = layouts;
    pipeDesc.bindGroupLayoutCount = 1;
    pipeDesc.label = "ore_split_stage_pipeline";

    std::string pipelineError;
    auto pipeline = ctx.makePipeline(pipeDesc, &pipelineError);
    INFO("makePipeline: " << pipelineError);
    REQUIRE(pipeline != nullptr);

    // The payoff: the fragment's own bindings resolve to native slots, so a
    // BindGroup for them can be built at all.
    static const Uniforms kLow = {0.3f, 0.0f, 0.0f, 0.0f};
    static const Uniforms kHigh = {0.0f, 0.6f, 0.0f, 0.0f};
    BufferDesc bufDesc{};
    bufDesc.usage = BufferUsage::uniform;
    bufDesc.size = sizeof(Uniforms);
    bufDesc.data = &kLow;
    auto lowBuf = ctx.makeBuffer(bufDesc);
    bufDesc.data = &kHigh;
    auto highBuf = ctx.makeBuffer(bufDesc);
    REQUIRE(lowBuf != nullptr);
    REQUIRE(highBuf != nullptr);

    BindGroupDesc::UBOEntry uboEntries[2]{};
    uboEntries[0].slot = layout->entries()[0].binding;
    uboEntries[0].buffer = lowBuf.get();
    uboEntries[0].size = sizeof(Uniforms);
    uboEntries[1].slot = layout->entries()[1].binding;
    uboEntries[1].buffer = highBuf.get();
    uboEntries[1].size = sizeof(Uniforms);
    BindGroupDesc bgDesc{};
    bgDesc.layout = layout.get();
    bgDesc.ubos = uboEntries;
    bgDesc.uboCount = 2;
    auto bindGroup = ctx.makeBindGroup(bgDesc);
    INFO("makeBindGroup: " << ctx.lastError());
    CHECK(bindGroup != nullptr);
}

// A sampler used only on depth textures must land in the layout as
// non-filtering — WebGPU rejects the filtering default against a depth
// texture. Covers the from-shader path, the merged-map shape the Luau lanes
// build through, and the intern split between flagged and unflagged twins.
void runDepthSamplerScenario(TestingWindow* window)
{
    auto* renderContext = window->renderContext();
    ore_gm::OreGMContext oreGM;
    REQUIRE(oreGM.ensureContext(renderContext));
    auto& ctx = *renderContext->getOreContext();

    auto depth = ore_gm::loadShader(ctx, ore_gm::kDepthSampleWitness);
    REQUIRE(depth.psModule);
    REQUIRE(!depth.psModule->m_textureSamplerPairs.empty());

    auto findSampler =
        [](const rcp<BindGroupLayout>& layout) -> const BindGroupLayoutEntry* {
        for (const BindGroupLayoutEntry& e : layout->entries())
        {
            if (e.kind == BindingKind::sampler)
                return &e;
        }
        return nullptr;
    };

    // From-shader path; group 1 holds the sampler + depth texture.
    auto fromShader =
        makeBindGroupLayoutFromShader(ctx, depth.psModule.get(), 1);
    REQUIRE(fromShader != nullptr);
    const BindGroupLayoutEntry* sampler = findSampler(fromShader);
    REQUIRE(sampler != nullptr);
    CHECK(sampler->samplerNonFiltering);

    // The salted intern id still dedupes flagged twins.
    CHECK(makeBindGroupLayoutFromShader(ctx, depth.psModule.get(), 1).get() ==
          fromShader.get());

    // Merged-map path with the pair sources the Luau lanes pass.
    auto vertex = ore_gm::loadShader(ctx, ore_gm::kTriangle);
    REQUIRE(vertex.vsModule);
    const BindingMap merged =
        bindingMapForStages(vertex.vsModule.get(), depth.psModule.get());
    auto mergedLayout = makeBindGroupLayoutFromBindingMap(ctx,
                                                          merged,
                                                          1,
                                                          nullptr,
                                                          0,
                                                          vertex.vsModule.get(),
                                                          depth.psModule.get());
    REQUIRE(mergedLayout != nullptr);
    sampler = findSampler(mergedLayout);
    REQUIRE(sampler != nullptr);
    CHECK(sampler->samplerNonFiltering);

    // The same map with no pair source interns an unflagged twin; the two
    // must not collapse into one entry.
    auto unflagged =
        makeBindGroupLayoutFromBindingMap(ctx, depth.psModule->m_bindingMap, 1);
    REQUIRE(unflagged != nullptr);
    sampler = findSampler(unflagged);
    REQUIRE(sampler != nullptr);
    CHECK(!sampler->samplerNonFiltering);
    CHECK(unflagged.get() != fromShader.get());
}

// Pairing is module state, so only the pair source tells two layouts of one
// binding shape apart. Two samplers in one group, each shader pairing a
// different one with the depth texture, must intern as different layouts.
void runDepthSamplerSetScenario(TestingWindow* window)
{
    auto* renderContext = window->renderContext();
    ore_gm::OreGMContext oreGM;
    REQUIRE(oreGM.ensureContext(renderContext));
    auto& ctx = *renderContext->getOreContext();

    BindingMap bm;
    auto push =
        [&](uint8_t binding, ResourceKind kind, TextureSampleType sampleType) {
            BindingMap::Entry e{};
            e.group = 1;
            e.binding = binding;
            e.kind = kind;
            e.stageMask = BindingMap::kStageFragment;
            e.backendSpace = 1;
            e.backendSlot[static_cast<size_t>(BindingMap::Stage::FS)] = binding;
            if (kind == ResourceKind::SampledTexture)
            {
                e.textureViewDim = TextureViewDim::D2;
                e.textureSampleType = sampleType;
            }
            bm.push(e);
        };
    push(0, ResourceKind::Sampler, {});
    push(1, ResourceKind::Sampler, {});
    push(2, ResourceKind::SampledTexture, TextureSampleType::Depth);
    push(3, ResourceKind::SampledTexture, TextureSampleType::Float);
    bm.finalize();
    bm.computeLayoutIds();
    REQUIRE(bm.layoutIdForGroup(1) != BindingMap::kNoLayoutId);

    struct PairSource : ShaderModule
    {};
    PairSource depthOnSampler0;
    depthOnSampler0.m_textureSamplerPairs = {{1, 2, 1, 0}, {1, 3, 1, 1}};
    PairSource depthOnSampler1;
    depthOnSampler1.m_textureSamplerPairs = {{1, 2, 1, 1}, {1, 3, 1, 0}};

    auto flagged = [](const rcp<BindGroupLayout>& layout, uint32_t binding) {
        for (const BindGroupLayoutEntry& e : layout->entries())
        {
            if (e.kind == BindingKind::sampler && e.binding == binding)
                return e.samplerNonFiltering;
        }
        return false;
    };

    auto first = makeBindGroupLayoutFromBindingMap(ctx,
                                                   bm,
                                                   1,
                                                   nullptr,
                                                   0,
                                                   nullptr,
                                                   &depthOnSampler0);
    REQUIRE(first != nullptr);
    CHECK(flagged(first, 0));
    CHECK(!flagged(first, 1));

    auto second = makeBindGroupLayoutFromBindingMap(ctx,
                                                    bm,
                                                    1,
                                                    nullptr,
                                                    0,
                                                    nullptr,
                                                    &depthOnSampler1);
    REQUIRE(second != nullptr);
    CHECK(second.get() != first.get());
    CHECK(!flagged(second, 0));
    CHECK(flagged(second, 1));

    // Same pairing again still dedupes.
    CHECK(makeBindGroupLayoutFromBindingMap(ctx,
                                            bm,
                                            1,
                                            nullptr,
                                            0,
                                            nullptr,
                                            &depthOnSampler0)
              .get() == first.get());
}
} // namespace

TEST_CASE("ore binds a fragment compiled apart from its vertex", "[ore]")
{
    int ran = 0;
    for (auto backend : kOreBackends)
    {
        if (!backendCanStart(backend))
            continue;
        auto* window = TestingWindow::Init(backend,
                                           {},
                                           TestingWindow::Visibility::headless);
        if (window == nullptr || window->renderContext() == nullptr ||
            !ore_gm::isOreBackendActive())
        {
            TestingWindow::Destroy();
            continue;
        }
        INFO("backend " << TestingWindow::BackendName(backend));
        runScenario(window);
        ++ran;
        TestingWindow::Destroy();
    }
    if (ran == 0)
        WARN("no Ore backend available headless; skipping");
}

TEST_CASE("ore layouts declare non-filtering samplers for depth-only use",
          "[ore]")
{
    int ran = 0;
    for (auto backend : kOreBackends)
    {
        if (!backendCanStart(backend))
            continue;
        auto* window = TestingWindow::Init(backend,
                                           {},
                                           TestingWindow::Visibility::headless);
        if (window == nullptr || window->renderContext() == nullptr ||
            !ore_gm::isOreBackendActive())
        {
            TestingWindow::Destroy();
            continue;
        }
        INFO("backend " << TestingWindow::BackendName(backend));
        runDepthSamplerScenario(window);
        runDepthSamplerSetScenario(window);
        ++ran;
        TestingWindow::Destroy();
    }
    if (ran == 0)
        WARN("no Ore backend available headless; skipping");
}
#endif // ORE_SPLIT_STAGE_TEST_ACTIVE
