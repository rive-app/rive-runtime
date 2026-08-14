/*
 * Copyright 2026 Rive
 */

// Confirms every make descriptor field, label string, and data blob round
// trips through the ordered ore stream with the caller's id and generation.
// GPU free, no real resources.

#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_make_recording.hpp"
#include "rive/renderer/ore/cmd/ore_make_replay.hpp"
#include "rive/renderer/ore/ore_context.hpp"

#include <catch.hpp>
#include <cstring>
#include <new>

using namespace rive::ore;
using namespace rive::ore::cmd;
using rive::Span;

namespace
{
Span<const uint8_t> blobOf(const OreCommandReader& r, BlobRef ref)
{
    return ref.absent() ? Span<const uint8_t>(nullptr, 0)
                        : r.blobAt(ref.offset, ref.size);
}
const char* cstrOf(const OreCommandReader& r, BlobRef ref)
{
    return reinterpret_cast<const char*>(blobOf(r, ref).data());
}
} // namespace

TEST_CASE("make stream records make* with the caller's ids", "[ore][cmd]")
{
    OreCommandBuffer cb;

    const uint32_t verts[4] = {1, 2, 3, 4};
    BufferDesc bd{};
    bd.usage = BufferUsage::vertex;
    bd.size = sizeof(verts);
    bd.data = verts;
    bd.immutable = true;
    bd.label = "vb";
    recordMakeBuffer(cb, 0, 1, bd);

    TextureDesc td{};
    td.width = 256;
    td.height = 128;
    td.depthOrArrayLayers = 1;
    td.format = TextureFormat::rgba8unorm;
    td.type = TextureType::texture2D;
    td.renderTarget = true;
    td.numMipmaps = 1;
    td.sampleCount = 4;
    td.label = "rt";
    recordMakeTexture(cb, 1, 1, td);

    SamplerDesc sd{};
    sd.minFilter = Filter::linear;
    sd.magFilter = Filter::nearest;
    sd.mipmapFilter = Filter::linear;
    sd.wrapU = WrapMode::repeat;
    sd.wrapV = WrapMode::clampToEdge;
    sd.wrapW = WrapMode::mirrorRepeat;
    sd.compare = CompareFunction::less;
    sd.minLod = 0.5f;
    sd.maxLod = 7.0f;
    sd.maxAnisotropy = 8;
    sd.label = nullptr; // null label must round trip as absent
    recordMakeSampler(cb, 2, 3, sd);

    OreCommandReader r(cb.commandBytes(), cb.blobBytes());
    CommandType t;

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makeBuffer);
    auto bh = r.read<MakeResourcePOD>();
    CHECK(bh.id == 0u);
    CHECK(bh.generation == 1u);
    auto b = r.read<BufferDescPOD>();
    CHECK(b.usage == BufferUsage::vertex);
    CHECK(b.size == sizeof(verts));
    CHECK(b.immutable);
    auto bData = blobOf(r, b.data);
    REQUIRE(bData.size() == sizeof(verts));
    CHECK(std::memcmp(bData.data(), verts, sizeof(verts)) == 0);
    auto bLabel = blobOf(r, b.label);
    REQUIRE(bLabel.size() == 3u); // "vb\0"
    CHECK(std::strcmp(cstrOf(r, b.label), "vb") == 0);

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makeTexture);
    auto th = r.read<MakeResourcePOD>();
    CHECK(th.id == 1u);
    auto tx = r.read<TextureDescPOD>();
    CHECK(tx.width == 256u);
    CHECK(tx.height == 128u);
    CHECK(tx.format == TextureFormat::rgba8unorm);
    CHECK(tx.type == TextureType::texture2D);
    CHECK(tx.renderTarget);
    CHECK(tx.sampleCount == 4u);
    CHECK(std::strcmp(cstrOf(r, tx.label), "rt") == 0);

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makeSampler);
    auto sh = r.read<MakeResourcePOD>();
    CHECK(sh.id == 2u);
    CHECK(sh.generation == 3u);
    auto s = r.read<SamplerDescPOD>();
    CHECK(s.minFilter == Filter::linear);
    CHECK(s.magFilter == Filter::nearest);
    CHECK(s.wrapU == WrapMode::repeat);
    CHECK(s.wrapW == WrapMode::mirrorRepeat);
    CHECK(s.compare == CompareFunction::less);
    CHECK(s.minLod == 0.5f);
    CHECK(s.maxLod == 7.0f);
    CHECK(s.maxAnisotropy == 8u);
    CHECK(s.label.absent());

    REQUIRE_FALSE(r.next(t));
}

TEST_CASE("make stream: a buffer with no initial data is absent, not empty",
          "[ore][cmd]")
{
    OreCommandBuffer cb;
    BufferDesc bd{};
    bd.usage = BufferUsage::uniform;
    bd.size = 64;
    bd.data = nullptr;
    recordMakeBuffer(cb, 0, 0, bd);

    OreCommandReader r(cb.commandBytes(), cb.blobBytes());
    CommandType t;
    REQUIRE(r.next(t));
    r.read<MakeResourcePOD>();
    auto b = r.read<BufferDescPOD>();
    CHECK(b.size == 64u);
    CHECK(b.data.absent());
    CHECK(blobOf(r, b.data).size() == 0u);
}

// Every ShaderModuleDesc field has to reach the wire, and the three places
// that carry it are hand written mirrors of this struct. Binding all members
// here fails to compile when one is added, which is the reminder to carry it
// through ShaderModuleDescPOD, recordMakeShaderModule and the replay side.
// State attached to a module outside this desc never crosses replay at all.
TEST_CASE("shader module desc fields are all accounted for", "[ore][cmd]")
{
    ShaderModuleDesc desc{};
    auto& [code,
           codeSize,
           language,
           stage,
           label,
           hlslSource,
           hlslSourceSize,
           hlslEntryPoint,
           bindingMapBytes,
           bindingMapSize,
           texSamplerPairBytes,
           texSamplerPairSize,
           glFixupBytes,
           glFixupSize,
           shaderAssetId] = desc;
    (void)code;
    (void)codeSize;
    (void)language;
    (void)stage;
    (void)label;
    (void)hlslSource;
    (void)hlslSourceSize;
    (void)hlslEntryPoint;
    (void)bindingMapBytes;
    (void)bindingMapSize;
    (void)texSamplerPairBytes;
    (void)texSamplerPairSize;
    (void)glFixupBytes;
    (void)glFixupSize;
    (void)shaderAssetId;
}

// Binding the members above only forces a compile break when a field is added;
// it does not force the field through the recorder and the replayer. Replaying
// a fully populated desc and comparing what the device is handed is what makes
// an omitted mirror fail.
namespace
{
class CapturingContext : public rive::ore::Context
{
public:
    CapturingContext() : Context(nullptr) {}

    bool captured = false;
    ShaderModuleDesc desc{};
    std::vector<uint8_t> code, bindingMap, texSamplerPair, glFixup;
    std::string label, hlslSource, hlslEntryPoint;

    rive::rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc& d) override
    {
        captured = true;
        desc = d;
        auto copy = [](const void* p, uint32_t n, std::vector<uint8_t>& out) {
            out.assign(static_cast<const uint8_t*>(p),
                       static_cast<const uint8_t*>(p) + n);
        };
        copy(d.code, d.codeSize, code);
        copy(d.bindingMapBytes, d.bindingMapSize, bindingMap);
        copy(d.texSamplerPairBytes, d.texSamplerPairSize, texSamplerPair);
        copy(d.glFixupBytes, d.glFixupSize, glFixup);
        label = d.label != nullptr ? d.label : "";
        hlslSource = d.hlslSource != nullptr ? d.hlslSource : "";
        hlslEntryPoint = d.hlslEntryPoint != nullptr ? d.hlslEntryPoint : "";
        return nullptr;
    }

    rive::rcp<Buffer> makeBuffer(const BufferDesc&) override { return nullptr; }
    rive::rcp<Texture> makeTexture(const TextureDesc&) override
    {
        return nullptr;
    }
    rive::rcp<TextureView> makeTextureView(const TextureViewDesc&) override
    {
        return nullptr;
    }
    rive::rcp<Sampler> makeSampler(const SamplerDesc&) override
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
    std::unique_ptr<RenderPass> beginRenderPass(const RenderPassDesc&,
                                                std::string*) override
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
    ShaderTarget shaderTarget() const override { return ShaderTarget::glsl; }
};
} // namespace

TEST_CASE("every shader module desc field survives record and replay",
          "[ore][cmd]")
{
    const uint8_t code[8] = {0xDE, 0xAD, 0xBE, 0xEF, 1, 2, 3, 4};
    const uint8_t bmap[3] = {9, 8, 7};
    const uint8_t pairs[8] = {1, 1, 0, 0, 1, 2, 0, 0};
    const uint8_t fixup[2] = {4, 5};

    ShaderModuleDesc sent{};
    sent.code = code;
    sent.codeSize = sizeof(code);
    sent.language = ShaderLanguage::wgsl;
    sent.stage = ShaderStage::fragment;
    sent.label = "every_field";
    sent.hlslSource = "float4 main() : SV_Target { return 0; }";
    sent.hlslSourceSize = (uint32_t)strlen(sent.hlslSource);
    sent.hlslEntryPoint = "main";
    sent.bindingMapBytes = bmap;
    sent.bindingMapSize = sizeof(bmap);
    sent.texSamplerPairBytes = pairs;
    sent.texSamplerPairSize = sizeof(pairs);
    sent.glFixupBytes = fixup;
    sent.glFixupSize = sizeof(fixup);
    sent.shaderAssetId = 4242;

    OreCommandBuffer cb;
    // Ids must append into an empty resident table.
    recordMakeShaderModule(cb, 0, 3, sent);

    CapturingContext ctx;
    OreResident table;
    OreCommandReader r(cb.commandBytes(), cb.blobBytes());
    CommandType t;
    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makeShaderModule);
    REQUIRE(replayOreLifecycle(
        ctx,
        table,
        t,
        r,
        [](ResourceHandle, OreKind) -> rive::gpu::GPUResource* {
            return nullptr;
        }));

    REQUIRE(ctx.captured);
    const ShaderModuleDesc& got = ctx.desc;
    CHECK(got.codeSize == sent.codeSize);
    CHECK(ctx.code == std::vector<uint8_t>(code, code + sizeof(code)));
    CHECK(got.language == sent.language);
    CHECK(got.stage == sent.stage);
    CHECK(ctx.label == "every_field");
    CHECK(ctx.hlslSource == sent.hlslSource);
    CHECK(ctx.hlslEntryPoint == "main");
    CHECK(got.bindingMapSize == sent.bindingMapSize);
    CHECK(ctx.bindingMap == std::vector<uint8_t>(bmap, bmap + sizeof(bmap)));
    CHECK(got.texSamplerPairSize == sent.texSamplerPairSize);
    CHECK(ctx.texSamplerPair ==
          std::vector<uint8_t>(pairs, pairs + sizeof(pairs)));
    CHECK(got.glFixupSize == sent.glFixupSize);
    CHECK(ctx.glFixup == std::vector<uint8_t>(fixup, fixup + sizeof(fixup)));
    CHECK(got.shaderAssetId == sent.shaderAssetId);
}

TEST_CASE("make stream records shader module, layout, view", "[ore][cmd]")
{
    OreCommandBuffer cb;

    const uint8_t code[8] = {0xDE, 0xAD, 0xBE, 0xEF, 1, 2, 3, 4};
    const uint8_t bmap[3] = {9, 8, 7};
    const uint8_t pairs[8] = {1, 1, 0, 0, 1, 2, 0, 0};
    const uint8_t fixup[2] = {4, 5};
    ShaderModuleDesc sm{};
    sm.code = code;
    sm.codeSize = sizeof(code);
    sm.language = ShaderLanguage::wgsl;
    sm.stage = ShaderStage::vertex;
    sm.bindingMapBytes = bmap;
    sm.bindingMapSize = sizeof(bmap);
    sm.texSamplerPairBytes = pairs;
    sm.texSamplerPairSize = sizeof(pairs);
    sm.glFixupBytes = fixup;
    sm.glFixupSize = sizeof(fixup);
    sm.shaderAssetId = 42;
    recordMakeShaderModule(cb, 0, 0, sm);

    BindGroupLayoutEntry entries[2]{};
    entries[0].binding = 0;
    entries[0].kind = BindingKind::uniformBuffer;
    entries[0].hasDynamicOffset = true;
    entries[1].binding = 1;
    entries[1].kind = BindingKind::sampledTexture;
    entries[1].nativeSlotFS = 5;
    BindGroupLayoutDesc bgl{};
    bgl.groupIndex = 2;
    bgl.entries = entries;
    bgl.entryCount = 2;
    recordMakeBindGroupLayout(cb, 1, 0, bgl);

    // The view references the texture by handle.
    TextureDesc td{};
    td.width = td.height = 64;
    recordMakeTexture(cb, 2, 0, td);
    TextureViewDesc tv{};
    tv.dimension = TextureViewDimension::texture2D;
    tv.baseMipLevel = 1;
    tv.mipCount = 2;
    recordMakeTextureView(cb, 3, 0, tv, 2);

    OreCommandReader r(cb.commandBytes(), cb.blobBytes());
    CommandType t;

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makeShaderModule);
    r.read<MakeResourcePOD>();
    auto s = r.read<ShaderModuleDescPOD>();
    CHECK(s.language == ShaderLanguage::wgsl);
    CHECK(s.stage == ShaderStage::vertex);
    CHECK(s.shaderAssetId == 42u);
    auto codeBlob = blobOf(r, s.code);
    REQUIRE(codeBlob.size() == sizeof(code));
    CHECK(std::memcmp(codeBlob.data(), code, sizeof(code)) == 0);
    auto bmapBlob = blobOf(r, s.bindingMapBytes);
    REQUIRE(bmapBlob.size() == sizeof(bmap));
    CHECK(std::memcmp(bmapBlob.data(), bmap, sizeof(bmap)) == 0);
    auto pairBlob = blobOf(r, s.texSamplerPairBytes);
    REQUIRE(pairBlob.size() == sizeof(pairs));
    CHECK(std::memcmp(pairBlob.data(), pairs, sizeof(pairs)) == 0);
    auto fixupBlob = blobOf(r, s.glFixupBytes);
    REQUIRE(fixupBlob.size() == sizeof(fixup));
    CHECK(std::memcmp(fixupBlob.data(), fixup, sizeof(fixup)) == 0);
    CHECK(s.hlslSource.absent());
    CHECK(s.label.absent());

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makeBindGroupLayout);
    r.read<MakeResourcePOD>();
    auto l = r.read<BindGroupLayoutDescPOD>();
    CHECK(l.groupIndex == 2u);
    CHECK(l.entryCount == 2u);
    auto entriesBlob = blobOf(r, l.entries);
    REQUIRE(entriesBlob.size() == 2 * sizeof(BindGroupLayoutEntry));
    const auto* outEntries =
        reinterpret_cast<const BindGroupLayoutEntry*>(entriesBlob.data());
    CHECK(outEntries[0].binding == 0u);
    CHECK(outEntries[0].hasDynamicOffset);
    CHECK(outEntries[1].binding == 1u);
    CHECK(outEntries[1].kind == BindingKind::sampledTexture);
    CHECK(outEntries[1].nativeSlotFS == 5u);

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makeTexture);
    r.read<MakeResourcePOD>();
    r.read<TextureDescPOD>();

    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makeTextureView);
    auto vh = r.read<MakeResourcePOD>();
    CHECK(vh.id == 3u);
    auto v = r.read<TextureViewDescPOD>();
    CHECK(v.texture == 2u);
    CHECK(v.baseMipLevel == 1u);
    CHECK(v.mipCount == 2u);
}

TEST_CASE("make stream records a pipeline with vertex layouts + refs",
          "[ore][cmd]")
{
    OreCommandBuffer cb;

    // Stand ins for handles recorded earlier.
    const ResourceHandle vsModule = 10, fsModule = 11, layout0 = 12,
                         layout1 = 13;

    VertexAttribute attrs[2]{};
    attrs[0] = {0, 0, VertexFormat::float2};
    attrs[1] = {8, 1, VertexFormat::float4};
    VertexBufferLayout vbl{};
    vbl.stride = 24;
    vbl.stepMode = VertexStepMode::vertex;
    vbl.attributes = attrs;
    vbl.attributeCount = 2;

    PipelineDesc pd{};
    pd.vertexEntryPoint = "vs_main";
    pd.fragmentEntryPoint = "fs_main";
    pd.vertexBuffers = &vbl;
    pd.vertexBufferCount = 1;
    pd.topology = PrimitiveTopology::triangleList;
    pd.colorTargets[0].format = TextureFormat::rgba8unorm;
    pd.colorTargets[0].blendEnabled = true;
    pd.colorCount = 1;
    pd.sampleCount = 4;
    pd.label = "pipe";
    ResourceHandle bglHandles[2] = {layout0, layout1};
    recordMakePipeline(cb,
                       0,
                       0,
                       pd,
                       vsModule,
                       fsModule,
                       Span<const ResourceHandle>(bglHandles, 2));

    OreCommandReader r(cb.commandBytes(), cb.blobBytes());
    CommandType t;
    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makePipeline);
    r.read<MakeResourcePOD>();
    auto p = r.read<PipelineDescPOD>();
    CHECK(p.vertexModule == vsModule);
    CHECK(p.fragmentModule == fsModule);
    CHECK(p.colorCount == 1u);
    CHECK(p.colorTargets[0].format == TextureFormat::rgba8unorm);
    CHECK(p.colorTargets[0].blendEnabled);
    CHECK(p.sampleCount == 4u);
    CHECK(std::strcmp(cstrOf(r, p.vertexEntryPoint), "vs_main") == 0);

    auto bglBlob = blobOf(r, p.bindGroupLayouts);
    REQUIRE(p.bindGroupLayoutCount == 2u);
    REQUIRE(bglBlob.size() == 2 * sizeof(ResourceHandle));
    const auto* bgl = reinterpret_cast<const ResourceHandle*>(bglBlob.data());
    CHECK(bgl[0] == layout0);
    CHECK(bgl[1] == layout1);

    REQUIRE(p.vertexBufferCount == 1u);
    auto vbBlob = blobOf(r, p.vertexBuffers);
    REQUIRE(vbBlob.size() == sizeof(VertexBufferLayoutPOD));
    const auto* vb =
        reinterpret_cast<const VertexBufferLayoutPOD*>(vbBlob.data());
    CHECK(vb[0].stride == 24u);
    CHECK(vb[0].attributeCount == 2u);
    auto attrBlob = blobOf(r, vb[0].attributes);
    REQUIRE(attrBlob.size() == 2 * sizeof(VertexAttribute));
    const auto* outAttrs =
        reinterpret_cast<const VertexAttribute*>(attrBlob.data());
    CHECK(outAttrs[0].format == VertexFormat::float2);
    CHECK(outAttrs[1].format == VertexFormat::float4);
    CHECK(outAttrs[1].offset == 8u);
    CHECK(outAttrs[1].shaderSlot == 1u);
}

TEST_CASE("make stream records a bind group with entry refs", "[ore][cmd]")
{
    OreCommandBuffer cb;
    const ResourceHandle layout = 5, buf0 = 6, view0 = 7, samp0 = 8;

    BindGroupDesc::UBOEntry ubo{};
    ubo.slot = 0;
    ubo.offset = 16;
    ubo.size = 256;
    BindGroupDesc::TexEntry tex{};
    tex.slot = 1;
    BindGroupDesc::SampEntry samp{};
    samp.slot = 2;

    BindGroupDesc bg{};
    bg.layout = nullptr; // unused, the ref is passed explicitly
    bg.ubos = &ubo;
    bg.uboCount = 1;
    bg.textures = &tex;
    bg.textureCount = 1;
    bg.samplers = &samp;
    bg.samplerCount = 1;
    bg.label = "bg";

    ResourceHandle uboH[1] = {buf0}, texH[1] = {view0}, sampH[1] = {samp0};
    recordMakeBindGroup(cb,
                        0,
                        0,
                        bg,
                        layout,
                        Span<const ResourceHandle>(uboH, 1),
                        Span<const ResourceHandle>(texH, 1),
                        Span<const ResourceHandle>(sampH, 1));

    OreCommandReader r(cb.commandBytes(), cb.blobBytes());
    CommandType t;
    REQUIRE(r.next(t));
    REQUIRE(t == CommandType::makeBindGroup);
    r.read<MakeResourcePOD>();
    auto b = r.read<BindGroupDescPOD>();
    CHECK(b.layout == layout);
    REQUIRE(b.uboCount == 1u);
    REQUIRE(b.textureCount == 1u);
    REQUIRE(b.samplerCount == 1u);

    const auto* ubos =
        reinterpret_cast<const UBOEntryPOD*>(blobOf(r, b.ubos).data());
    CHECK(ubos[0].slot == 0u);
    CHECK(ubos[0].buffer == buf0);
    CHECK(ubos[0].offset == 16u);
    CHECK(ubos[0].size == 256u);
    const auto* texs =
        reinterpret_cast<const TexEntryPOD*>(blobOf(r, b.textures).data());
    CHECK(texs[0].slot == 1u);
    CHECK(texs[0].view == view0);
    const auto* samps =
        reinterpret_cast<const SampEntryPOD*>(blobOf(r, b.samplers).data());
    CHECK(samps[0].slot == 2u);
    CHECK(samps[0].sampler == samp0);
}

namespace
{
// Descriptors reach the recorder on a caller's stack, so their padding holds
// whatever was there. Default init, not value init, so the member
// initializers run and the gaps keep the fill.
template <typename Desc, typename Fn> void withFilledDesc(uint8_t fill, Fn&& fn)
{
    alignas(Desc) uint8_t storage[sizeof(Desc)];
    std::memset(storage, fill, sizeof(storage));
    Desc* d = new (storage) Desc;
    fn(*d);
    d->~Desc();
}

#if defined(_MSC_VER)
#define ORE_NEVER_INLINE __declspec(noinline)
#else
#define ORE_NEVER_INLINE __attribute__((noinline))
#endif

// Leaves a deep stack region dirty so the PODs the recorder builds land on
// used memory rather than on a fresh page. Inlining would put the scratch in
// the caller's frame, which the recorder never reaches.
ORE_NEVER_INLINE void dirtyStack()
{
    volatile uint8_t scratch[4096];
    for (size_t i = 0; i < sizeof(scratch); ++i)
    {
        scratch[i] = 0xAB;
    }
}

const uint32_t kVerts[4] = {1, 2, 3, 4};
const uint8_t kCode[8] = {1, 2, 3, 4, 5, 6, 7, 8};

void recordOneOfEach(uint8_t fill, OreCommandBuffer& cb)
{
    withFilledDesc<BufferDesc>(fill, [&](BufferDesc& d) {
        d.usage = BufferUsage::vertex;
        d.size = sizeof(kVerts);
        d.data = kVerts;
        d.immutable = true;
        d.label = "vb";
        recordMakeBuffer(cb, 0, 1, d);
    });
    withFilledDesc<TextureDesc>(fill, [&](TextureDesc& d) {
        d.width = 256;
        d.height = 128;
        d.format = TextureFormat::rgba8unorm;
        d.renderTarget = true;
        d.numMipmaps = 1;
        d.sampleCount = 4;
        d.label = "rt";
        recordMakeTexture(cb, 1, 1, d);
    });
    withFilledDesc<SamplerDesc>(fill, [&](SamplerDesc& d) {
        d.minFilter = Filter::linear;
        d.magFilter = Filter::nearest;
        d.compare = CompareFunction::less;
        d.minLod = 0.5f;
        d.maxLod = 7.0f;
        d.maxAnisotropy = 8;
        recordMakeSampler(cb, 2, 1, d);
    });
    withFilledDesc<ShaderModuleDesc>(fill, [&](ShaderModuleDesc& d) {
        d.code = kCode;
        d.codeSize = sizeof(kCode);
        d.language = ShaderLanguage::wgsl;
        d.stage = ShaderStage::fragment;
        d.label = "fs";
        recordMakeShaderModule(cb, 3, 1, d);
    });
    withFilledDesc<BindGroupLayoutDesc>(fill, [&](BindGroupLayoutDesc& d) {
        // Entries reach the blob arena as raw structs, so a filled array is
        // the only way to catch a gap in the entry type.
        BindGroupLayoutEntry entries[2];
        std::memset(entries, fill, sizeof(entries));
        for (auto& e : entries)
        {
            new (&e) BindGroupLayoutEntry;
        }
        entries[0].binding = 0;
        entries[0].kind = BindingKind::uniformBuffer;
        entries[0].hasDynamicOffset = true;
        entries[0].minBindingSize = 64;
        entries[1].binding = 1;
        entries[1].kind = BindingKind::sampledTexture;
        entries[1].nativeSlotFS = 5;

        d.groupIndex = 2;
        d.entries = entries;
        d.entryCount = 2;
        d.label = "bgl";
        recordMakeBindGroupLayout(cb, 4, 1, d);
    });
    withFilledDesc<TextureViewDesc>(fill, [&](TextureViewDesc& d) {
        d.dimension = TextureViewDimension::texture2D;
        d.aspect = TextureAspect::all;
        d.mipCount = 1;
        d.layerCount = 1;
        recordMakeTextureView(cb, 5, 1, d, 1);
    });
    withFilledDesc<PipelineDesc>(fill, [&](PipelineDesc& d) {
        // Attributes reach the blob arena as raw structs too.
        VertexAttribute attrs[2];
        std::memset(attrs, fill, sizeof(attrs));
        for (auto& a : attrs)
        {
            new (&a) VertexAttribute;
        }
        attrs[0].format = VertexFormat::float2;
        attrs[1].format = VertexFormat::float4;
        attrs[1].offset = 8;
        attrs[1].shaderSlot = 1;
        VertexBufferLayout vbl{};
        vbl.stride = 24;
        vbl.attributes = attrs;
        vbl.attributeCount = 2;
        d.vertexBuffers = &vbl;
        d.vertexBufferCount = 1;

        d.vertexEntryPoint = "vs_main";
        d.fragmentEntryPoint = "fs_main";
        d.colorTargets[0].format = TextureFormat::rgba8unorm;
        d.colorTargets[0].blendEnabled = true;
        d.colorCount = 1;
        d.depthStencil.format = TextureFormat::depth24plusStencil8;
        d.depthStencil.depthWriteEnabled = true;
        d.depthStencil.depthBias = 2;
        d.stencilFront.compare = CompareFunction::equal;
        d.sampleCount = 4;
        d.label = "pipe";
        const ResourceHandle bgls[1] = {4};
        recordMakePipeline(cb,
                           6,
                           1,
                           d,
                           3,
                           3,
                           Span<const ResourceHandle>(bgls, 1));
    });
    withFilledDesc<BindGroupDesc>(fill, [&](BindGroupDesc& d) {
        BindGroupDesc::UBOEntry ubo{};
        ubo.slot = 0;
        ubo.size = 256;
        d.ubos = &ubo;
        d.uboCount = 1;
        d.label = "bg";
        const ResourceHandle ubos[1] = {0};
        recordMakeBindGroup(cb,
                            7,
                            1,
                            d,
                            4,
                            Span<const ResourceHandle>(ubos, 1),
                            Span<const ResourceHandle>(nullptr, 0),
                            Span<const ResourceHandle>(nullptr, 0));
    });
}
} // namespace

TEST_CASE("make stream records equal resources byte for byte", "[ore][cmd]")
{
    // The other recording tests compare in silver form, which normalizes
    // padding away, so only raw bytes catch a gap reaching the stream.
    OreCommandBuffer clean, dirty;
    recordOneOfEach(0x00, clean);
    dirtyStack();
    recordOneOfEach(0xAB, dirty);

    auto a = clean.commandBytes(), b = dirty.commandBytes();
    REQUIRE(a.size() == b.size());
    CHECK(std::memcmp(a.data(), b.data(), a.size()) == 0);

    auto ba = clean.blobBytes(), bb = dirty.blobBytes();
    REQUIRE(ba.size() == bb.size());
    CHECK(std::memcmp(ba.data(), bb.data(), ba.size()) == 0);
}

TEST_CASE("make stream reset reuses the buffer", "[ore][cmd]")
{
    OreCommandBuffer cb;
    TextureDesc td{};
    td.width = td.height = 16;
    recordMakeTexture(cb, 0, 0, td);
    recordMakeTexture(cb, 1, 0, td);
    CHECK_FALSE(cb.empty());

    cb.reset();
    CHECK(cb.empty());
    recordMakeTexture(cb, 0, 1, td);
    CHECK_FALSE(cb.empty());
}
