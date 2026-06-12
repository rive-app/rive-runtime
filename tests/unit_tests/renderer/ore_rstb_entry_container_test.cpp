#include "rive/renderer/ore/ore_rstb_entry_container.hpp"
#include <catch.hpp>
#include <cstring>
#include <string>

using namespace rive::ore;

static std::string spanStr(const uint8_t* p, uint32_t n)
{
    return std::string(reinterpret_cast<const char*>(p), n);
}

TEST_CASE("rstb-whole-module-container-round-trip", "[rstb-entry]")
{
    // Two vertex entries + one fragment entry; physical differs from logical
    // (the MSL-rename case).
    std::vector<RstbEntry> entries = {
        {0, "vertex", "vertex_1", {}},
        {0, "instanced", "instanced", {}},
        {1, "fragment", "fragment_1", {}},
    };
    const std::string src = "// shared MSL/SPIRV/WGSL source\n";
    auto blob =
        buildWholeModuleContainer(entries,
                                  reinterpret_cast<const uint8_t*>(src.data()),
                                  static_cast<uint32_t>(src.size()));

    std::vector<RstbEntryView> out;
    const uint8_t* outSrc = nullptr;
    uint32_t outSrcLen = 0;
    REQUIRE(parseWholeModuleContainer(blob.data(),
                                      static_cast<uint32_t>(blob.size()),
                                      out,
                                      &outSrc,
                                      &outSrcLen));
    REQUIRE(out.size() == 3);
    CHECK(out[0].stage == 0);
    CHECK(out[0].logical == "vertex");
    CHECK(out[0].physical == "vertex_1");
    CHECK(out[1].logical == "instanced");
    CHECK(out[1].physical == "instanced");
    CHECK(out[2].stage == 1);
    CHECK(out[2].logical == "fragment");
    CHECK(out[2].physical == "fragment_1");
    CHECK(spanStr(outSrc, outSrcLen) == src);
    // Whole-module entries do not carry per-entry source.
    CHECK(out[0].source == nullptr);
}

TEST_CASE("rstb-per-entry-container-round-trip", "[rstb-entry]")
{
    std::vector<RstbEntry> entries;
    RstbEntry vs;
    vs.stage = 0;
    vs.logical = "vs_main";
    vs.physical = "main";
    const std::string vsSrc = "void main() { /* vs */ }";
    vs.source.assign(vsSrc.begin(), vsSrc.end());
    entries.push_back(vs);
    RstbEntry fs;
    fs.stage = 1;
    fs.logical = "fs_main";
    fs.physical = "main";
    const std::string fsSrc = "void main() { /* fs longer source */ }";
    fs.source.assign(fsSrc.begin(), fsSrc.end());
    entries.push_back(fs);

    auto blob = buildPerEntryContainer(entries);

    std::vector<RstbEntryView> out;
    REQUIRE(parsePerEntryContainer(blob.data(),
                                   static_cast<uint32_t>(blob.size()),
                                   out));
    REQUIRE(out.size() == 2);
    CHECK(out[0].logical == "vs_main");
    CHECK(out[0].physical == "main");
    CHECK(spanStr(out[0].source, out[0].sourceSize) == vsSrc);
    CHECK(out[1].logical == "fs_main");
    CHECK(spanStr(out[1].source, out[1].sourceSize) == fsSrc);
}

TEST_CASE("rstb-container-empty-entries", "[rstb-entry]")
{
    // Zero entries (e.g. a not-yet-populated module) must round-trip cleanly.
    std::vector<RstbEntry> none;
    const std::string src = "src";
    auto blob =
        buildWholeModuleContainer(none,
                                  reinterpret_cast<const uint8_t*>(src.data()),
                                  static_cast<uint32_t>(src.size()));
    std::vector<RstbEntryView> out;
    const uint8_t* outSrc = nullptr;
    uint32_t outSrcLen = 0;
    REQUIRE(parseWholeModuleContainer(blob.data(),
                                      static_cast<uint32_t>(blob.size()),
                                      out,
                                      &outSrc,
                                      &outSrcLen));
    CHECK(out.empty());
    CHECK(spanStr(outSrc, outSrcLen) == src);
}

TEST_CASE("rstb-container-rejects-truncation", "[rstb-entry]")
{
    std::vector<RstbEntry> entries = {{0, "vs_main", "vs_main", {}}};
    const std::string src = "source bytes";
    auto blob =
        buildWholeModuleContainer(entries,
                                  reinterpret_cast<const uint8_t*>(src.data()),
                                  static_cast<uint32_t>(src.size()));

    // Every truncated prefix must be rejected by the bounded readers, never
    // read out of bounds.
    for (uint32_t n = 0; n < blob.size(); n++)
    {
        std::vector<RstbEntryView> out;
        const uint8_t* outSrc = nullptr;
        uint32_t outSrcLen = 0;
        CHECK_FALSE(parseWholeModuleContainer(blob.data(),
                                              n,
                                              out,
                                              &outSrc,
                                              &outSrcLen));
    }
    // The full blob parses.
    std::vector<RstbEntryView> out;
    const uint8_t* outSrc = nullptr;
    uint32_t outSrcLen = 0;
    CHECK(parseWholeModuleContainer(blob.data(),
                                    static_cast<uint32_t>(blob.size()),
                                    out,
                                    &outSrc,
                                    &outSrcLen));
}