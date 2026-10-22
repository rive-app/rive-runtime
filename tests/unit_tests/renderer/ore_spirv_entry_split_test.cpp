#include "gm/ore_gm_shaders.rstb.hpp"
#include "ore/vulkan/ore_spirv_entry_split.hpp"
#include "rive/renderer/ore/ore_shader_asset_modules.hpp"
#include <algorithm>
#include <catch.hpp>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

using namespace rive::ore;

namespace
{
// naga 30 SPIR-V for this WGSL, two vertex and two fragment entry points:
//   @group(0) @binding(0) var<uniform> tint: vec4f;
//   fn shade(c: vec3f) -> vec4f { return vec4f(c, 1.0) * tint; }
//   struct FogOut { @builtin(position) position: vec4f,
//                   @location(0) uv: vec2f };
//   @vertex fn fog_vs(@builtin(vertex_index) i: u32) -> FogOut {
//       return FogOut(vec4f(f32(i), 0.0, 0.0, 1.0), vec2f(0.5)); }
//   @fragment fn fog_fs(v: FogOut) -> @location(0) vec4f {
//       return shade(vec3f(v.uv, 0.0)); }
//   struct DustOut { @builtin(position) position: vec4f,
//                    @location(0) shade: f32, @location(1) world: vec3f };
//   @vertex fn dust_vs(@location(0) seed: f32) -> DustOut {
//       return DustOut(vec4f(seed), seed, vec3f(seed)); }
//   @fragment fn dust_fs(v: DustOut) -> @location(0) vec4f {
//       return shade(v.world * v.shade); }
const std::vector<uint32_t> kTwoPairs = {
    0x07230203, 0x00010000, 0x0000001c, 0x00000064, 0x00000000, 0x00020011,
    0x00000001, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x0008000f, 0x00000000,
    0x00000021, 0x5f676f66, 0x00007376, 0x0000001a, 0x0000001d, 0x0000001f,
    0x0008000f, 0x00000004, 0x0000003a, 0x5f676f66, 0x00007366, 0x00000033,
    0x00000036, 0x00000039, 0x0009000f, 0x00000000, 0x00000048, 0x74737564,
    0x0073765f, 0x00000041, 0x00000044, 0x00000045, 0x00000046, 0x0009000f,
    0x00000004, 0x0000005d, 0x74737564, 0x0073665f, 0x00000055, 0x00000057,
    0x00000059, 0x0000005c, 0x00030010, 0x0000003a, 0x00000007, 0x00030010,
    0x0000005d, 0x00000007, 0x00050048, 0x00000007, 0x00000000, 0x00000023,
    0x00000000, 0x00050048, 0x00000007, 0x00000001, 0x00000023, 0x00000010,
    0x00050048, 0x00000009, 0x00000000, 0x00000023, 0x00000000, 0x00050048,
    0x00000009, 0x00000001, 0x00000023, 0x00000010, 0x00050048, 0x00000009,
    0x00000002, 0x00000023, 0x00000020, 0x00040047, 0x0000000a, 0x00000022,
    0x00000000, 0x00040047, 0x0000000a, 0x00000021, 0x00000000, 0x00030047,
    0x0000000b, 0x00000002, 0x00050048, 0x0000000b, 0x00000000, 0x00000023,
    0x00000000, 0x00040047, 0x0000001a, 0x0000000b, 0x0000002a, 0x00040047,
    0x0000001d, 0x0000000b, 0x00000000, 0x00040047, 0x0000001f, 0x0000001e,
    0x00000000, 0x00040047, 0x00000033, 0x0000000b, 0x0000000f, 0x00040047,
    0x00000036, 0x0000001e, 0x00000000, 0x00040047, 0x00000039, 0x0000001e,
    0x00000000, 0x00040047, 0x00000041, 0x0000001e, 0x00000000, 0x00040047,
    0x00000044, 0x0000000b, 0x00000000, 0x00040047, 0x00000045, 0x0000001e,
    0x00000000, 0x00040047, 0x00000046, 0x0000001e, 0x00000001, 0x00040047,
    0x00000055, 0x0000000b, 0x0000000f, 0x00040047, 0x00000057, 0x0000001e,
    0x00000000, 0x00040047, 0x00000059, 0x0000001e, 0x00000001, 0x00040047,
    0x0000005c, 0x0000001e, 0x00000000, 0x00020013, 0x00000002, 0x00030016,
    0x00000004, 0x00000020, 0x00040017, 0x00000003, 0x00000004, 0x00000004,
    0x00040017, 0x00000005, 0x00000004, 0x00000003, 0x00040017, 0x00000006,
    0x00000004, 0x00000002, 0x0004001e, 0x00000007, 0x00000003, 0x00000006,
    0x00040015, 0x00000008, 0x00000020, 0x00000000, 0x0005001e, 0x00000009,
    0x00000003, 0x00000004, 0x00000005, 0x0003001e, 0x0000000b, 0x00000003,
    0x00040020, 0x0000000c, 0x00000002, 0x0000000b, 0x0004003b, 0x0000000c,
    0x0000000a, 0x00000002, 0x00040021, 0x00000010, 0x00000003, 0x00000005,
    0x00040020, 0x00000011, 0x00000002, 0x00000003, 0x0004002b, 0x00000008,
    0x00000012, 0x00000000, 0x0004002b, 0x00000004, 0x00000014, 0x3f800000,
    0x00040020, 0x0000001b, 0x00000001, 0x00000008, 0x0004003b, 0x0000001b,
    0x0000001a, 0x00000001, 0x00040020, 0x0000001e, 0x00000003, 0x00000003,
    0x0004003b, 0x0000001e, 0x0000001d, 0x00000003, 0x00040020, 0x00000020,
    0x00000003, 0x00000006, 0x0004003b, 0x00000020, 0x0000001f, 0x00000003,
    0x00030021, 0x00000022, 0x00000002, 0x0004002b, 0x00000004, 0x00000023,
    0x00000000, 0x0004002b, 0x00000004, 0x00000024, 0x3f000000, 0x0005002c,
    0x00000006, 0x00000025, 0x00000024, 0x00000024, 0x00040020, 0x0000002b,
    0x00000003, 0x00000004, 0x0004002b, 0x00000008, 0x0000002c, 0x00000001,
    0x00040020, 0x00000034, 0x00000001, 0x00000003, 0x0004003b, 0x00000034,
    0x00000033, 0x00000001, 0x00040020, 0x00000037, 0x00000001, 0x00000006,
    0x0004003b, 0x00000037, 0x00000036, 0x00000001, 0x0004003b, 0x0000001e,
    0x00000039, 0x00000003, 0x00040020, 0x00000042, 0x00000001, 0x00000004,
    0x0004003b, 0x00000042, 0x00000041, 0x00000001, 0x0004003b, 0x0000001e,
    0x00000044, 0x00000003, 0x0004003b, 0x0000002b, 0x00000045, 0x00000003,
    0x00040020, 0x00000047, 0x00000003, 0x00000005, 0x0004003b, 0x00000047,
    0x00000046, 0x00000003, 0x0004003b, 0x00000034, 0x00000055, 0x00000001,
    0x0004003b, 0x00000042, 0x00000057, 0x00000001, 0x00040020, 0x0000005a,
    0x00000001, 0x00000005, 0x0004003b, 0x0000005a, 0x00000059, 0x00000001,
    0x0004003b, 0x0000001e, 0x0000005c, 0x00000003, 0x00050036, 0x00000003,
    0x0000000f, 0x00000000, 0x00000010, 0x00030037, 0x00000005, 0x0000000e,
    0x000200f8, 0x0000000d, 0x00050041, 0x00000011, 0x00000013, 0x0000000a,
    0x00000012, 0x000200f9, 0x00000015, 0x000200f8, 0x00000015, 0x00050050,
    0x00000003, 0x00000016, 0x0000000e, 0x00000014, 0x0004003d, 0x00000003,
    0x00000017, 0x00000013, 0x00050085, 0x00000003, 0x00000018, 0x00000016,
    0x00000017, 0x000200fe, 0x00000018, 0x00010038, 0x00050036, 0x00000002,
    0x00000021, 0x00000000, 0x00000022, 0x000200f8, 0x00000019, 0x0004003d,
    0x00000008, 0x0000001c, 0x0000001a, 0x000200f9, 0x00000026, 0x000200f8,
    0x00000026, 0x00040070, 0x00000004, 0x00000027, 0x0000001c, 0x00070050,
    0x00000003, 0x00000028, 0x00000027, 0x00000023, 0x00000023, 0x00000014,
    0x00050050, 0x00000007, 0x00000029, 0x00000028, 0x00000025, 0x00050051,
    0x00000003, 0x0000002a, 0x00000029, 0x00000000, 0x0003003e, 0x0000001d,
    0x0000002a, 0x00050041, 0x0000002b, 0x0000002d, 0x0000001d, 0x0000002c,
    0x0004003d, 0x00000004, 0x0000002e, 0x0000002d, 0x0004007f, 0x00000004,
    0x0000002f, 0x0000002e, 0x0003003e, 0x0000002d, 0x0000002f, 0x00050051,
    0x00000006, 0x00000030, 0x00000029, 0x00000001, 0x0003003e, 0x0000001f,
    0x00000030, 0x000100fd, 0x00010038, 0x00050036, 0x00000002, 0x0000003a,
    0x00000000, 0x00000022, 0x000200f8, 0x00000031, 0x0004003d, 0x00000003,
    0x00000035, 0x00000033, 0x0004003d, 0x00000006, 0x00000038, 0x00000036,
    0x00050050, 0x00000007, 0x00000032, 0x00000035, 0x00000038, 0x00050041,
    0x00000011, 0x0000003b, 0x0000000a, 0x00000012, 0x000200f9, 0x0000003c,
    0x000200f8, 0x0000003c, 0x00050051, 0x00000006, 0x0000003d, 0x00000032,
    0x00000001, 0x00050050, 0x00000005, 0x0000003e, 0x0000003d, 0x00000023,
    0x00050039, 0x00000003, 0x0000003f, 0x0000000f, 0x0000003e, 0x0003003e,
    0x00000039, 0x0000003f, 0x000100fd, 0x00010038, 0x00050036, 0x00000002,
    0x00000048, 0x00000000, 0x00000022, 0x000200f8, 0x00000040, 0x0004003d,
    0x00000004, 0x00000043, 0x00000041, 0x000200f9, 0x00000049, 0x000200f8,
    0x00000049, 0x00070050, 0x00000003, 0x0000004a, 0x00000043, 0x00000043,
    0x00000043, 0x00000043, 0x00060050, 0x00000005, 0x0000004b, 0x00000043,
    0x00000043, 0x00000043, 0x00060050, 0x00000009, 0x0000004c, 0x0000004a,
    0x00000043, 0x0000004b, 0x00050051, 0x00000003, 0x0000004d, 0x0000004c,
    0x00000000, 0x0003003e, 0x00000044, 0x0000004d, 0x00050041, 0x0000002b,
    0x0000004e, 0x00000044, 0x0000002c, 0x0004003d, 0x00000004, 0x0000004f,
    0x0000004e, 0x0004007f, 0x00000004, 0x00000050, 0x0000004f, 0x0003003e,
    0x0000004e, 0x00000050, 0x00050051, 0x00000004, 0x00000051, 0x0000004c,
    0x00000001, 0x0003003e, 0x00000045, 0x00000051, 0x00050051, 0x00000005,
    0x00000052, 0x0000004c, 0x00000002, 0x0003003e, 0x00000046, 0x00000052,
    0x000100fd, 0x00010038, 0x00050036, 0x00000002, 0x0000005d, 0x00000000,
    0x00000022, 0x000200f8, 0x00000053, 0x0004003d, 0x00000003, 0x00000056,
    0x00000055, 0x0004003d, 0x00000004, 0x00000058, 0x00000057, 0x0004003d,
    0x00000005, 0x0000005b, 0x00000059, 0x00060050, 0x00000009, 0x00000054,
    0x00000056, 0x00000058, 0x0000005b, 0x00050041, 0x00000011, 0x0000005e,
    0x0000000a, 0x00000012, 0x000200f9, 0x0000005f, 0x000200f8, 0x0000005f,
    0x00050051, 0x00000005, 0x00000060, 0x00000054, 0x00000002, 0x00050051,
    0x00000004, 0x00000061, 0x00000054, 0x00000001, 0x0005008e, 0x00000005,
    0x00000062, 0x00000060, 0x00000061, 0x00050039, 0x00000003, 0x00000063,
    0x0000000f, 0x00000062, 0x0003003e, 0x0000005c, 0x00000063, 0x000100fd,
    0x00010038,
};

constexpr uint32_t kOpUndef = 1;
constexpr uint32_t kOpName = 5;
constexpr uint32_t kOpExtInstImport = 11;
constexpr uint32_t kOpEntryPoint = 15;
constexpr uint32_t kOpExecutionMode = 16;
constexpr uint32_t kOpCapability = 17;
constexpr uint32_t kOpTypeVoid = 19;
constexpr uint32_t kOpTypePipe = 38;
constexpr uint32_t kOpConstantTrue = 41;
constexpr uint32_t kOpSpecConstantOp = 52;
constexpr uint32_t kOpFunction = 54;
constexpr uint32_t kOpFunctionCall = 57;
constexpr uint32_t kOpVariable = 59;
constexpr uint32_t kOpDecorate = 71;
constexpr uint32_t kOpMemberDecorate = 72;
constexpr uint32_t kOpDecorationGroup = 73;
constexpr uint32_t kOpDecorateId = 332;
constexpr uint32_t kOpLabel = 248;
constexpr uint32_t kDecorationLocation = 30;
constexpr uint32_t kDecorationAlignmentId = 46;
constexpr uint32_t kCapabilityFunctionPointersINTEL = 5603;
constexpr uint32_t kStorageInput = 1;
constexpr uint32_t kStorageOutput = 3;

struct Shape
{
  std::vector<std::string> entries;
  int functions = 0;
  int locations = 0;
  int executionModes = 0;
  int interfaceVariables = 0;
  int names = 0;
  // Every name, decoration, execution mode, interface and call target is
  // still declared.
  bool targetsDeclared = true;
};

Shape shapeOf(const std::vector<uint32_t>& words)
{
  Shape shape;
  std::vector<uint32_t> declared;
  std::vector<uint32_t> targets;
  for (size_t at = 5; at < words.size(); at += words[at] >> 16)
  {
    const uint32_t* w = &words[at];
    const uint32_t length = w[0] >> 16;
    const uint32_t op = w[0] & 0xFFFF;
    if ((op >= kOpTypeVoid && op <= kOpTypePipe) || op == kOpExtInstImport)
      declared.push_back(w[1]);
    if ((op >= kOpConstantTrue && op <= kOpSpecConstantOp) || op == kOpUndef ||
        op == kOpVariable || op == kOpFunction)
      declared.push_back(w[2]);
    if (op == kOpName || op == kOpDecorate || op == kOpMemberDecorate ||
        op == kOpExecutionMode)
      targets.push_back(w[1]);
    if (op == kOpFunctionCall)
      targets.push_back(w[3]);
    if (op == kOpEntryPoint)
    {
      const char* name = reinterpret_cast<const char*>(&w[3]);
      shape.entries.emplace_back(name);
      targets.push_back(w[2]);
      targets.insert(
          targets.end(),
          w + 3 + strlen(name) / sizeof(uint32_t) + 1,
          w + length);
    }
    shape.functions += op == kOpFunction;
    shape.names += op == kOpName;
    shape.locations += op == kOpDecorate && w[2] == kDecorationLocation;
    shape.executionModes += op == kOpExecutionMode;
    shape.interfaceVariables += op == kOpVariable && (w[3] == kStorageInput ||
                                                      w[3] == kStorageOutput);
  }
  for (uint32_t target : targets)
  {
    if (std::find(declared.begin(), declared.end(), target) == declared.end())
      shape.targetsDeclared = false;
  }
  return shape;
}

// Where sections of a module end, and ids worth aiming at.
struct Landmarks
{
  size_t afterImports = 0;
  size_t firstAnnotation = 0;
  size_t firstBody = 0;
  uint32_t voidType = 0;
  uint32_t label = 0;
  uint32_t fogVs = 0;
  uint32_t dustFs = 0;
};

Landmarks landmarksOf(const std::vector<uint32_t>& words)
{
  Landmarks marks;
  for (size_t at = 5; at < words.size(); at += words[at] >> 16)
  {
    const uint32_t* w = &words[at];
    const uint32_t op = w[0] & 0xFFFF;
    if (op == kOpExtInstImport)
      marks.afterImports = at + (w[0] >> 16);
    if ((op == kOpDecorate || op == kOpMemberDecorate) &&
        marks.firstAnnotation == 0)
      marks.firstAnnotation = at;
    if (op == kOpTypeVoid)
      marks.voidType = w[1];
    if (op == kOpLabel && marks.label == 0)
    {
      marks.label = w[1];
      marks.firstBody = at + (w[0] >> 16);
    }
    if (op == kOpEntryPoint)
    {
      std::string name = reinterpret_cast<const char*>(&w[3]);
      if (name == "fog_vs")
        marks.fogVs = w[2];
      if (name == "dust_fs")
        marks.dustFs = w[2];
    }
  }
  return marks;
}

// The fixture with one instruction inserted at a word offset, and the id
// bound raised so the instruction may declare a fresh id.
std::vector<uint32_t> inserted(size_t at, std::vector<uint32_t> instruction)
{
  std::vector<uint32_t> words = kTwoPairs;
  words[3]++;
  words.insert(words.begin() + at, instruction.begin(), instruction.end());
  return words;
}

std::vector<uint32_t> cut(
    const std::vector<uint32_t>& words,
    uint32_t model,
    const char* name)
{
  SpirvEntryScratch scratch;
  return spirvEntryModule(words.data(), words.size(), model, name, scratch)
             ? scratch.words
             : std::vector<uint32_t>{};
}
} // namespace

TEST_CASE("spirv-entry-split-keeps-what-each-entry-reaches", "[ore-spirv]")
{
  CHECK(spirvEntryPointCount(kTwoPairs.data(), kTwoPairs.size()) == 4);

  struct Expected
  {
    uint32_t model;
    const char* name;
    int functions;
    int locations;
    int executionModes;
    int interfaceVariables;
  };
  // Fragment entries also reach shade(); each keeps only its own varyings and
  // execution modes.
  for (const Expected& expected :
       {Expected{kSpirvVertexModel, "fog_vs", 1, 1, 0, 3},
        Expected{kSpirvFragmentModel, "fog_fs", 2, 2, 1, 3},
        Expected{kSpirvVertexModel, "dust_vs", 1, 3, 0, 4},
        Expected{kSpirvFragmentModel, "dust_fs", 2, 3, 1, 4}})
  {
    Shape shape = shapeOf(cut(kTwoPairs, expected.model, expected.name));
    CHECK(shape.entries == std::vector<std::string>{expected.name});
    CHECK(shape.functions == expected.functions);
    CHECK(shape.locations == expected.locations);
    CHECK(shape.executionModes == expected.executionModes);
    CHECK(shape.interfaceVariables == expected.interfaceVariables);
    CHECK(shape.targetsDeclared);
  }
}

TEST_CASE("spirv-entry-split-reuses-its-scratch", "[ore-spirv]")
{
  // Code at any alignment counts the same.
  std::vector<uint8_t> unaligned(kTwoPairs.size() * sizeof(uint32_t) + 1);
  memcpy(
      unaligned.data() + 1,
      kTwoPairs.data(),
      kTwoPairs.size() * sizeof(uint32_t));
  CHECK(spirvEntryPointCount(unaligned.data() + 1, kTwoPairs.size()) == 4);

  // One scratch across cuts of every entry, the smaller cut module in between
  // included, gives what a fresh scratch does.
  SpirvEntryScratch scratch;
  const std::vector<uint32_t> smaller = cut(
      kTwoPairs,
      kSpirvFragmentModel,
      "fog_fs");
  for (const auto& [words, model, name] :
       {std::tuple(kTwoPairs, kSpirvVertexModel, "fog_vs"),
        std::tuple(kTwoPairs, kSpirvFragmentModel, "dust_fs"),
        std::tuple(smaller, kSpirvFragmentModel, "fog_fs"),
        std::tuple(kTwoPairs, kSpirvFragmentModel, "fog_fs"),
        std::tuple(kTwoPairs, kSpirvVertexModel, "dust_vs")})
  {
    INFO(name);
    const bool cuts = spirvEntryModule(
        words.data(),
        words.size(),
        model,
        name,
        scratch);
    const std::vector<uint32_t> fresh = cut(words, model, name);
    CHECK(cuts == !fresh.empty());
    if (cuts)
      CHECK(scratch.words == fresh);
  }
}

TEST_CASE("spirv-entry-split-leaves-what-it-cannot-cut", "[ore-spirv]")
{
  // An entry of another stage or name is not there to cut to.
  CHECK(cut(kTwoPairs, kSpirvFragmentModel, "fog_vs").empty());
  CHECK(cut(kTwoPairs, kSpirvVertexModel, "missing").empty());

  // A module already down to one entry point stays whole.
  auto alone = cut(kTwoPairs, kSpirvVertexModel, "fog_vs");
  CHECK(spirvEntryPointCount(alone.data(), alone.size()) == 1);
  CHECK(cut(alone, kSpirvVertexModel, "fog_vs").empty());

  const Landmarks marks = landmarksOf(kTwoPairs);
  REQUIRE(
      (marks.afterImports && marks.firstAnnotation && marks.firstBody &&
       marks.voidType && marks.label && marks.fogVs && marks.dustFs));
  const uint32_t freshId = kTwoPairs[3];

  // A decoration on an id inside a function cannot be accounted for.
  const uint32_t relaxedPrecision = 0;
  CHECK(cut(inserted(
                marks.firstAnnotation,
                {(3u << 16) | kOpDecorate, marks.label, relaxedPrecision}),
            kSpirvVertexModel,
            "fog_vs")
            .empty());

  // Nor can an id decoration naming a function the cut drops, or groups.
  CHECK(cut(inserted(
                marks.firstAnnotation,
                {(4u << 16) | kOpDecorateId,
                 marks.fogVs,
                 kDecorationAlignmentId,
                 marks.dustFs}),
            kSpirvVertexModel,
            "fog_vs")
            .empty());
  CHECK(cut(inserted(
                marks.firstAnnotation,
                {(2u << 16) | kOpDecorationGroup, freshId}),
            kSpirvVertexModel,
            "fog_vs")
            .empty());

  // A debug name there is dropped instead, as the cut drops every name.
  Shape shape = shapeOf(
      cut(inserted(
              marks.firstAnnotation,
              {(3u << 16) | kOpName, marks.label, uint32_t('x')}),
          kSpirvVertexModel,
          "fog_vs"));
  CHECK(shape.entries == std::vector<std::string>{"fog_vs"});
  CHECK(shape.names == 0);
  CHECK(shape.targetsDeclared);

  // Sets other than GLSL.std.450 may reference what the cut drops.
  uint32_t set[6] = {};
  memcpy(set, "OpenCL.DebugInfo.100", 20);
  std::vector<uint32_t> import = {(8u << 16) | kOpExtInstImport, freshId};
  import.insert(import.end(), set, set + 6);
  CHECK(cut(inserted(marks.afterImports, import), kSpirvVertexModel, "fog_vs")
            .empty());

  // So may instructions that name functions without calling them.
  CHECK(cut(inserted(
                5,
                {(2u << 16) | kOpCapability, kCapabilityFunctionPointersINTEL}),
            kSpirvVertexModel,
            "fog_vs")
            .empty());

  // Malformed modules: an unterminated entry name, a function inside another,
  // and ids past the bound.
  uint32_t unterminated[2];
  memcpy(unterminated, "abcdefgh", 8);
  CHECK(cut(inserted(
                marks.firstAnnotation,
                {(5u << 16) | kOpEntryPoint,
                 kSpirvVertexModel,
                 marks.fogVs,
                 unterminated[0],
                 unterminated[1]}),
            kSpirvVertexModel,
            "fog_vs")
            .empty());
  CHECK(cut(inserted(
                marks.firstBody,
                {(5u << 16) | kOpFunction,
                 marks.voidType,
                 freshId,
                 0,
                 marks.voidType}),
            kSpirvVertexModel,
            "fog_vs")
            .empty());
  std::vector<uint32_t> overBound = kTwoPairs;
  overBound[3] = static_cast<uint32_t>(overBound.size() + 1);
  CHECK(cut(overBound, kSpirvVertexModel, "fog_vs").empty());

  CHECK(cut({}, kSpirvVertexModel, "fog_vs").empty());
  std::vector<uint32_t> truncated(kTwoPairs.begin(), kTwoPairs.end() - 1);
  CHECK(cut(truncated, kSpirvVertexModel, "fog_vs").empty());
}

// A naga that starts decorating ids inside functions would make every cut
// bail, and Adreno fail again, without any other test noticing.
TEST_CASE("spirv-entry-split-cuts-every-baked-test-shader", "[ore-spirv]")
{
  int multiEntryModules = 0;
  for (uint32_t id = 0; id < ore_gm_shaders::kShaderCount; id++)
  {
    INFO(ore_gm_shaders::kShaderNames[id]);
    const uint32_t begin = ore_gm_shaders::kShaderOffsets[id];
    rive::ShaderAsset asset;
    REQUIRE(decodeBareRstb(
        asset,
        ore_gm_shaders::kShaderData + begin,
        ore_gm_shaders::kShaderOffsets[id + 1] - begin));
    bool visited = visitShaderAssetModules(
        ShaderTarget::spirv,
        asset,
        [&](const ShaderModuleDesc& desc,
            uint32_t,
            uint32_t,
            const std::vector<RstbEntryView>& entries) {
          std::vector<uint32_t> words(desc.codeSize / sizeof(uint32_t));
          memcpy(words.data(), desc.code, words.size() * sizeof(uint32_t));
          if (spirvEntryPointCount(words.data(), words.size()) < 2)
            return true;
          multiEntryModules++;
          for (const RstbEntryView& entry : entries)
          {
            if (entry.stage > 1)
              continue;
            INFO(entry.physical);
            Shape shape = shapeOf(
                cut(words,
                    entry.stage == 0 ? kSpirvVertexModel : kSpirvFragmentModel,
                    entry.physical.c_str()));
            CHECK(shape.entries == std::vector<std::string>{entry.physical});
            CHECK(shape.targetsDeclared);
          }
          return true;
        });
    CHECK(visited);
  }
  // A bundle rebaked one entry per module would leave this test vacuous.
  CHECK(multiEntryModules > 0);
}
