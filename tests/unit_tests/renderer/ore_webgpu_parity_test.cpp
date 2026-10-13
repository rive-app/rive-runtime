/*
 * Copyright 2026 Rive
 */

// Dawn's bind group and layout checks run on every backend. The deferred
// context validates at record time, so no GPU is needed.

#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"

#include <catch.hpp>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::ore;
using namespace rive::ore::cmd;

namespace
{
using SampleType = BindGroupLayoutEntry::SampleType;

BindGroupLayoutEntry entry(uint32_t binding, BindingKind kind)
{
    BindGroupLayoutEntry e{};
    e.binding = binding;
    e.kind = kind;
    return e;
}

BindGroupLayoutEntry texture(
    uint32_t binding,
    SampleType type = SampleType::floatFilterable,
    TextureViewDimension dimension = TextureViewDimension::texture2D)
{
    BindGroupLayoutEntry e = entry(binding, BindingKind::sampledTexture);
    e.textureSampleType = type;
    e.textureViewDim = dimension;
    return e;
}

rcp<BindGroupLayout> layoutOf(Context& ctx,
                              const std::vector<BindGroupLayoutEntry>& entries)
{
    BindGroupLayoutDesc desc;
    desc.entries = entries.data();
    desc.entryCount = static_cast<uint32_t>(entries.size());
    return ctx.makeBindGroupLayout(desc);
}

rcp<TextureView> viewOf(Context& ctx,
                        TextureFormat format,
                        uint32_t sampleCount = 1,
                        TextureAspect aspect = TextureAspect::all)
{
    TextureDesc td;
    td.width = 4;
    td.height = 4;
    td.format = format;
    td.sampleCount = sampleCount;
    rcp<Texture> tex = ctx.makeTexture(td);
    TextureViewDesc vd;
    vd.texture = tex.get();
    vd.aspect = aspect;
    return ctx.makeTextureView(vd);
}

std::string textureError(DeferredOreContext& ctx,
                         const BindGroupLayoutEntry& layoutEntry,
                         const rcp<TextureView>& view)
{
    auto layout = layoutOf(ctx, {layoutEntry});
    BindGroupDesc::TexEntry tex{layoutEntry.binding, view.get()};
    BindGroupDesc desc;
    desc.layout = layout.get();
    desc.textures = &tex;
    desc.textureCount = 1;
    ctx.clearLastError();
    return ctx.makeBindGroup(desc) != nullptr ? "" : ctx.lastError();
}

std::string samplerError(DeferredOreContext& ctx,
                         const BindGroupLayoutEntry& layoutEntry,
                         const SamplerDesc& samplerDesc)
{
    auto layout = layoutOf(ctx, {layoutEntry});
    auto sampler = ctx.makeSampler(samplerDesc);
    BindGroupDesc::SampEntry samp{layoutEntry.binding, sampler.get()};
    BindGroupDesc desc;
    desc.layout = layout.get();
    desc.samplers = &samp;
    desc.samplerCount = 1;
    ctx.clearLastError();
    return ctx.makeBindGroup(desc) != nullptr ? "" : ctx.lastError();
}

bool contains(const std::string& s, const char* part)
{
    return s.find(part) != std::string::npos;
}
} // namespace

TEST_CASE("a bind group fills every layout entry exactly once", "[ore][webgpu]")
{
    DeferredOreContext ctx(nullptr);
    auto layout = layoutOf(ctx,
                           {entry(0, BindingKind::uniformBuffer),
                            texture(1),
                            entry(2, BindingKind::sampler)});
    BufferDesc bd{};
    bd.usage = BufferUsage::uniform;
    bd.size = 256;
    auto buffer = ctx.makeBuffer(bd);
    BindGroupDesc::UBOEntry ubo{0, buffer.get(), 0, 0};
    BindGroupDesc desc;
    desc.layout = layout.get();
    desc.ubos = &ubo;
    desc.uboCount = 1;

    CHECK(ctx.makeBindGroup(desc) == nullptr);
    CHECK(contains(ctx.lastError(), "@group(0) @binding(1)"));
    CHECK(contains(ctx.lastError(), "leaves this texture empty"));

    auto view = viewOf(ctx, TextureFormat::rgba8unorm);
    auto sampler = ctx.makeSampler({});
    BindGroupDesc::TexEntry tex{1, view.get()};
    BindGroupDesc::SampEntry samps[2] = {{2, sampler.get()},
                                         {2, sampler.get()}};
    desc.textures = &tex;
    desc.textureCount = 1;
    desc.samplers = samps;
    desc.samplerCount = 2;
    CHECK(ctx.makeBindGroup(desc) == nullptr);
    CHECK(contains(ctx.lastError(), "more than once"));

    desc.samplerCount = 1;
    ctx.clearLastError();
    CHECK(ctx.makeBindGroup(desc) != nullptr);
}

TEST_CASE("an entry with no resource is refused", "[ore][webgpu]")
{
    DeferredOreContext ctx(nullptr);
    auto layout = layoutOf(ctx, {entry(0, BindingKind::sampler)});
    BindGroupDesc::SampEntry samp{0, nullptr};
    BindGroupDesc desc;
    desc.layout = layout.get();
    desc.samplers = &samp;
    desc.samplerCount = 1;
    CHECK(ctx.makeBindGroup(desc) == nullptr);
    CHECK(contains(ctx.lastError(), "@binding(0): binds no sampler"));
}

TEST_CASE("an entry the layout lacks is refused", "[ore][webgpu]")
{
    DeferredOreContext ctx(nullptr);
    auto layout = layoutOf(ctx, {entry(0, BindingKind::sampler)});
    auto sampler = ctx.makeSampler({});
    BindGroupDesc::SampEntry samps[2] = {{0, sampler.get()},
                                         {1, sampler.get()}};
    BindGroupDesc desc;
    desc.layout = layout.get();
    desc.samplers = samps;
    desc.samplerCount = 2;
    CHECK(ctx.makeBindGroup(desc) == nullptr);
    CHECK(contains(ctx.lastError(), "@binding(1): the layout has no sampler"));
}

TEST_CASE("uniform buffers need uniform usage and 256 byte offsets",
          "[ore][webgpu]")
{
    DeferredOreContext ctx(nullptr);
    auto layout = layoutOf(ctx, {entry(0, BindingKind::uniformBuffer)});
    auto bind = [&](BufferUsage usage, uint32_t offset) -> std::string {
        BufferDesc bd{};
        bd.usage = usage;
        bd.size = 1024;
        auto buffer = ctx.makeBuffer(bd);
        BindGroupDesc::UBOEntry ubo{0, buffer.get(), offset, 64};
        BindGroupDesc desc;
        desc.layout = layout.get();
        desc.ubos = &ubo;
        desc.uboCount = 1;
        ctx.clearLastError();
        return ctx.makeBindGroup(desc) != nullptr ? "" : ctx.lastError();
    };
    CHECK(bind(BufferUsage::uniform, 256).empty());
    CHECK(contains(bind(BufferUsage::uniform, 64), "not a multiple of 256"));
    CHECK(contains(bind(BufferUsage::vertex, 0), "without uniform usage"));
}

TEST_CASE("samplers fill only bindings of their own type", "[ore][webgpu]")
{
    DeferredOreContext ctx(nullptr);
    SamplerDesc linear;
    linear.minFilter = Filter::linear;
    linear.magFilter = Filter::linear;
    SamplerDesc compare;
    compare.compare = CompareFunction::less;
    SamplerDesc linearCompare = linear;
    linearCompare.compare = CompareFunction::less;

    BindGroupLayoutEntry nonFiltering = entry(0, BindingKind::sampler);
    nonFiltering.samplerNonFiltering = true;
    CHECK(contains(samplerError(ctx, nonFiltering, linear),
                   "give it nearest filters"));
    CHECK(samplerError(ctx, nonFiltering, SamplerDesc{}).empty());

    BindGroupLayoutEntry filtering = entry(0, BindingKind::sampler);
    CHECK(samplerError(ctx, filtering, linear).empty());
    CHECK(contains(samplerError(ctx, filtering, compare),
                   "binds a comparison sampler"));

    BindGroupLayoutEntry comparison = entry(0, BindingKind::comparisonSampler);
    CHECK(contains(samplerError(ctx, comparison, SamplerDesc{}),
                   "has no compare function"));
    CHECK(samplerError(ctx, comparison, linearCompare).empty());
}

TEST_CASE("a view fills only entries its format can be read as",
          "[ore][webgpu]")
{
    DeferredOreContext ctx(nullptr);
    auto r32 = viewOf(ctx, TextureFormat::r32float);
    CHECK(contains(textureError(ctx, texture(0), r32),
                   "cannot read this view's format as a float texture"));
    CHECK(textureError(ctx, texture(0, SampleType::floatUnfilterable), r32)
              .empty());

    auto depth = viewOf(ctx, TextureFormat::depth32float);
    CHECK(textureError(ctx, texture(0, SampleType::depth), depth).empty());
    CHECK(textureError(ctx, texture(0, SampleType::floatUnfilterable), depth)
              .empty());
    CHECK(contains(textureError(ctx, texture(0), depth), "as a float texture"));

    auto color = viewOf(ctx, TextureFormat::rgba8unorm);
    CHECK(contains(textureError(ctx, texture(0, SampleType::depth), color),
                   "as a depth texture"));

    auto both = viewOf(ctx, TextureFormat::depth24plusStencil8);
    CHECK(contains(textureError(ctx, texture(0, SampleType::depth), both),
                   "both depth and stencil"));
    auto depthOnly = viewOf(ctx,
                            TextureFormat::depth24plusStencil8,
                            1,
                            TextureAspect::depthOnly);
    CHECK(textureError(ctx, texture(0, SampleType::depth), depthOnly).empty());
}

TEST_CASE("a view matches the declared dimension and sample count",
          "[ore][webgpu]")
{
    DeferredOreContext ctx(nullptr);
    auto flat = viewOf(ctx, TextureFormat::rgba8unorm);
    CHECK(contains(textureError(ctx,
                                texture(0,
                                        SampleType::floatFilterable,
                                        TextureViewDimension::array2D),
                                flat),
                   "binds a 2d view where the shader declares a 2d array"));

    auto msaa = viewOf(ctx, TextureFormat::rgba8unorm, 4);
    CHECK(contains(textureError(ctx, texture(0), msaa),
                   "binds a multisampled view"));
}

TEST_CASE("pipeline layouts stay within WebGPU's per stage limits",
          "[ore][webgpu]")
{
    std::vector<BindGroupLayoutEntry> samplers;
    for (uint32_t i = 0; i < 17; ++i)
        samplers.push_back(entry(i, BindingKind::sampler));
    DeferredOreContext ctx(nullptr);
    auto wide = layoutOf(ctx, samplers);
    BindGroupLayout* layouts[] = {wide.get()};
    std::string error;
    CHECK_FALSE(validateStageLimits(layouts, 1, &error));
    CHECK(contains(error, "vertex stage 17 samplers"));

    samplers.pop_back();
    auto full = layoutOf(ctx, samplers);
    layouts[0] = full.get();
    CHECK(validateStageLimits(layouts, 1, &error));

    std::vector<BindGroupLayoutEntry> dynamic;
    for (uint32_t i = 0; i < 9; ++i)
    {
        dynamic.push_back(entry(i, BindingKind::uniformBuffer));
        dynamic.back().hasDynamicOffset = true;
    }
    auto offsets = layoutOf(ctx, dynamic);
    layouts[0] = offsets.get();
    CHECK_FALSE(validateStageLimits(layouts, 1, &error));
    CHECK(contains(error, "9 dynamic uniform buffers"));
}

// Following WebGPU's auto layout, so a 32 bit float texture read only with
// textureLoad binds on the web.
TEST_CASE("float textures no sampler reads become unfilterable",
          "[ore][webgpu]")
{
    BindingMap bm;
    auto push = [&](uint8_t binding, ResourceKind kind) {
        BindingMap::Entry e{};
        e.group = 0;
        e.binding = binding;
        e.kind = kind;
        e.stageMask = BindingMap::kStageFragment;
        e.backendSlot[static_cast<size_t>(BindingMap::Stage::FS)] = binding;
        if (kind == ResourceKind::SampledTexture)
        {
            e.textureViewDim = TextureViewDim::D2;
            e.textureSampleType = TextureSampleType::Float;
        }
        bm.push(e);
    };
    push(0, ResourceKind::SampledTexture);
    push(1, ResourceKind::SampledTexture);
    push(2, ResourceKind::Sampler);
    bm.finalize();
    bm.computeLayoutIds();

    struct PairSource : ShaderModule
    {};
    PairSource sampledFirst;
    sampledFirst.m_textureSamplerPairs = {{0, 0, 0, 2}};

    auto sampleTypeOf = [](const rcp<BindGroupLayout>& layout, uint32_t b) {
        return layout->findEntry(b)->textureSampleType;
    };
    DeferredOreContext ctx(nullptr);
    auto inferred = makeBindGroupLayoutFromBindingMap(ctx,
                                                      bm,
                                                      0,
                                                      nullptr,
                                                      0,
                                                      nullptr,
                                                      &sampledFirst);
    REQUIRE(inferred != nullptr);
    CHECK(sampleTypeOf(inferred, 0) == SampleType::floatFilterable);
    CHECK(sampleTypeOf(inferred, 1) == SampleType::floatUnfilterable);

    // Without a pair source the pairings are unknown, so nothing changes, and
    // the two layouts must not intern as one.
    auto unknown = makeBindGroupLayoutFromBindingMap(ctx, bm, 0);
    REQUIRE(unknown != nullptr);
    CHECK(sampleTypeOf(unknown, 1) == SampleType::floatFilterable);
    CHECK(unknown.get() != inferred.get());
}

// A layout built from a file that never samples a texture declares it
// unfilterable, so a pipeline whose shader samples it must be refused.
TEST_CASE("a pipeline cannot sample an unfilterable texture with filtering",
          "[ore][webgpu]")
{
    struct PairSource : ShaderModule
    {};
    PairSource shader;
    shader.m_textureSamplerPairs = {{0, 1, 0, 2}};

    DeferredOreContext ctx(nullptr);
    auto validate = [&](SampleType type, bool nonFiltering) {
        BindGroupLayoutEntry samp = entry(2, BindingKind::sampler);
        samp.samplerNonFiltering = nonFiltering;
        auto layout = layoutOf(ctx, {texture(1, type), samp});
        BindGroupLayout* layouts[] = {layout.get()};
        PipelineDesc desc;
        desc.vertexModule = &shader;
        desc.fragmentModule = &shader;
        desc.bindGroupLayouts = layouts;
        desc.bindGroupLayoutCount = 1;
        std::string error;
        validatePipelineDesc(desc,
                             BindingMap(),
                             NativeSlotScope::perStage,
                             &error);
        return error;
    };
    CHECK(contains(validate(SampleType::floatUnfilterable, false),
                   "@binding(1): the layout declares this texture "
                   "unfilterable"));
    CHECK(validate(SampleType::floatUnfilterable, true).empty());
    CHECK(validate(SampleType::floatFilterable, false).empty());
}
