// A shader reading a multisampled texture, which WebGL2 cannot compile. A
// device without the feature refuses it when the module is made, naming the
// shader and binding; one with it tells the author once that WebGL2 needs a
// fallback.

#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "utils/no_op_factory.hpp"

#if defined(RIVE_CANVAS) && defined(RIVE_ORE) && defined(WITH_RIVE_TOOLS)
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_rstb_binding_names.hpp"
#include "rive/renderer/ore/ore_shader_asset_modules.hpp"

#include <unordered_set>

#include <cstring>

using namespace rive;

namespace
{
// A bare GLSL RSTB whose fs_main reads a multisampled depth texture at
// @group(0) @binding(1) beside an fs_fallback that does not, with the entry
// bindings and, when named, the binding names sections a current bake writes.
std::vector<uint8_t> multisampledRstb(bool named, bool used = true)
{
  ore::BindingMap map;
  ore::BindingMap::Entry e{};
  e.group = 0;
  e.binding = 1;
  e.kind = ore::ResourceKind::SampledTexture;
  e.stageMask = ore::BindingMap::kStageFragment;
  e.backendSlot[1] = 0;
  e.textureViewDim = ore::TextureViewDim::D2;
  e.textureSampleType = ore::TextureSampleType::Depth;
  e.textureMultisampled = true;
  map.push(e);
  map.finalize();
  map.computeLayoutIds();
  std::vector<uint8_t> mapBlob = map.toBlob();

  const char* source = "void main() {}";
  std::vector<uint8_t> glsl{2};
  for (const char* entry : {"fs_main", "fs_fallback"})
  {
    glsl.push_back(1);
    ore::rstb_detail::putStr(glsl, entry);
    ore::rstb_detail::putStr(glsl, entry);
    ore::rstb_detail::putU32(glsl, (uint32_t)strlen(source));
    glsl.insert(glsl.end(), source, source + strlen(source));
  }

  ore::RstbEntryBindings reads;
  reads.stage = 1;
  reads.name = "fs_main";
  if (used)
  {
    reads.bindings = {0 << 8 | 1};
  }
  ore::RstbEntryBindings fallback;
  fallback.stage = 1;
  fallback.name = "fs_fallback";
  std::vector<uint8_t> entryBindings = ore::encodeEntryBindings(
      {reads, fallback});

  std::vector<uint8_t> names = named ? ore::encodeBindingNames(
                                           {{0, 1, "gbDepth"}})
                                     : std::vector<uint8_t>{};

  std::vector<uint8_t> rstb;
  ore::rstb_detail::putU32(rstb, 0x52535442u);
  ore::rstb_detail::putU16(rstb, 4);
  rstb.push_back(2);
  rstb.push_back(named ? 2 : 1);
  rstb.push_back(1);
  ore::rstb_detail::putU32(rstb, 0);
  ore::rstb_detail::putU32(rstb, (uint32_t)glsl.size());
  rstb.push_back(11);
  ore::rstb_detail::putU32(rstb, (uint32_t)glsl.size());
  ore::rstb_detail::putU32(rstb, (uint32_t)mapBlob.size());
  rstb.push_back(ore::kRstbEntryBindingsTag);
  ore::rstb_detail::putU16(rstb, (uint16_t)entryBindings.size());
  rstb.insert(rstb.end(), entryBindings.begin(), entryBindings.end());
  if (named)
  {
    rstb.push_back(ore::kRstbBindingNamesTag);
    ore::rstb_detail::putU16(rstb, (uint16_t)names.size());
    rstb.insert(rstb.end(), names.begin(), names.end());
  }
  rstb.insert(rstb.end(), glsl.begin(), glsl.end());
  rstb.insert(rstb.end(), mapBlob.begin(), mapBlob.end());
  return rstb;
}

ore::ReplayCaps glslCaps(bool multisampledTextureRead)
{
  ore::ReplayCaps caps;
  caps.featuresKnown = true;
  caps.shaderTarget = ore::ShaderTarget::glsl;
  caps.features.multisampledTextureRead = multisampledTextureRead;
  return caps;
}

class RecordingFactory : public NoOpFactory
{
public:
  explicit RecordingFactory(const ore::ReplayCaps& caps) : oreContext(caps) {}
  ore::cmd::DeferredOreContext oreContext;
  ore::Context* ore() override { return &oreContext; }
};

// Loads the shader `count` times in init and returns whether each load gave
// a shader.
bool loadShader(ScriptingTest& vm, int count)
{
  ScriptedObjectTest scriptedObjectTest;
  lua_State* L = vm.state();
  lua_pushinteger(L, count);
  lua_setglobal(L, "loads");
  lua_getglobal(L, "init");
  lua_pushvalue(L, -2);
  lua_newrive<ScriptedContext>(L, &scriptedObjectTest);
  if (lua_pcall(L, 2, 1, 0) != LUA_OK)
  {
    FAIL(lua_tostring(L, -1));
  }
  return lua_toboolean(L, -1);
}

const char* kLoadScript = R"(
function init(self, context)
  local all = true
  for i = 1, loads do
    all = all and context:shader('fog') ~= nil
  end
  return all
end
)";
} // namespace

TEST_CASE(
    "a device without multisampled reads refuses the shader by name",
    "[scripting][gpu][multisampled]")
{
  RecordingFactory factory(glslCaps(false));
  ScriptingTest vm(kLoadScript, 1, false, {}, true, &factory);
  vm.vm()->context()->registerShaderRstb("fog", multisampledRstb(true));
  CHECK_FALSE(loadShader(vm, 1));
  REQUIRE(vm.errors.size() == 1);
  CHECK(
      vm.errors[0] ==
      "shader 'fog' reads multisampled texture 'gbDepth', which this device "
      "cannot (WebGL2); check features().multisampledTextureRead and give it "
      "a fallback");
  CHECK(vm.console.empty());
}

TEST_CASE(
    "an older bake without binding names is refused by slot",
    "[scripting][gpu][multisampled]")
{
  RecordingFactory factory(glslCaps(false));
  ScriptingTest vm(kLoadScript, 1, false, {}, true, &factory);
  vm.vm()->context()->registerShaderRstb("fog", multisampledRstb(false));
  CHECK_FALSE(loadShader(vm, 1));
  REQUIRE(vm.errors.size() == 1);
  CHECK(
      vm.errors[0].find("texture '@group(0) @binding(1)'") !=
      std::string::npos);
}

TEST_CASE(
    "a device with multisampled reads tells the author once per shader",
    "[scripting][gpu][multisampled]")
{
  RecordingFactory factory(glslCaps(true));
  ScriptingTest vm(kLoadScript, 1, false, {}, true, &factory);
  vm.vm()->context()->registerShaderRstb("fog", multisampledRstb(true));
  CHECK(loadShader(vm, 3));
  CHECK(vm.errors.empty());
  REQUIRE(vm.console.size() == 1);
  CHECK(
      vm.console[0] ==
      "shader 'fog' reads multisampled texture 'gbDepth'. WebGL2 cannot read "
      "multisampled textures, so to run there this project needs a fallback "
      "gated on features().multisampledTextureRead.");
}

TEST_CASE(
    "an unbound recording neither refuses nor tells",
    "[scripting][gpu][multisampled]")
{
  RecordingFactory factory(ore::ReplayCaps{});
  ScriptingTest vm(kLoadScript, 1, false, {}, true, &factory);
  vm.vm()->context()->registerShaderRstb("fog", multisampledRstb(true));
  CHECK(loadShader(vm, 1));
  CHECK(vm.errors.empty());
  CHECK(vm.console.empty());
}
TEST_CASE(
    "a fallback entry builds beside one that reads a multisampled texture",
    "[scripting][gpu][multisampled]")
{
  // Per entry modules let the wasm lane mint the fallback a refusal asks for.
  ShaderAsset asset;
  std::vector<uint8_t> rstb = multisampledRstb(true);
  REQUIRE(ore::decodeBareRstb(asset, rstb.data(), (uint32_t)rstb.size()));
  std::vector<ore::RstbEntryView> entries;
  auto blob = asset.findShader((uint8_t)ore::ShaderTarget::glsl);
  REQUIRE(
      ore::parsePerEntryContainer(blob.data(), (uint32_t)blob.size(), entries));
  REQUIRE(entries.size() == 2);

  RecordingFactory factory(glslCaps(false));
  std::unordered_set<std::string> noticed;
  std::string notice;
  CHECK_FALSE(ore::admitMultisampledReads(
      factory.oreContext,
      asset,
      "fog",
      noticed,
      &notice,
      &entries[0]));
  CHECK(ore::admitMultisampledReads(
      factory.oreContext,
      asset,
      "fog",
      noticed,
      &notice,
      &entries[1]));
  CHECK(
      ore::multisampledTextureReads(ore::ShaderTarget::glsl, asset) ==
      std::vector<std::string>{"gbDepth"});
}
TEST_CASE(
    "a multisampled texture no entry uses is not a read",
    "[scripting][gpu][multisampled]")
{
  RecordingFactory factory(glslCaps(false));
  ScriptingTest vm(kLoadScript, 1, false, {}, true, &factory);
  vm.vm()->context()->registerShaderRstb(
      "fog",
      multisampledRstb(true, /*used=*/false));
  CHECK(loadShader(vm, 1));
  CHECK(vm.errors.empty());
}

TEST_CASE(
    "shaders sharing a name each tell the author",
    "[scripting][gpu][multisampled]")
{
  RecordingFactory factory(glslCaps(true));
  std::vector<uint8_t> rstb = multisampledRstb(true);
  std::unordered_set<std::string> noticed;
  for (uint32_t id : {7u, 9u})
  {
    ShaderAsset asset;
    REQUIRE(ore::decodeBareRstb(asset, rstb.data(), (uint32_t)rstb.size()));
    asset.assetId(id);
    std::string notice;
    CHECK(ore::admitMultisampledReads(
        factory.oreContext,
        asset,
        "fog",
        noticed,
        &notice));
    CHECK_FALSE(notice.empty());
  }
}
#endif
