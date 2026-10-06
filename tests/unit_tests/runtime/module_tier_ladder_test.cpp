#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/module_tier_ladder.hpp"

#include <catch.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unistd.h>

using namespace rive;

namespace
{

// (module (func (export "f") (result i32) i32.const 42))
static const uint8_t kTinyModule[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // magic + version
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,       // type () -> i32
    0x03, 0x02, 0x01, 0x00,                         // func 0 uses type 0
    0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,       // export "f"
    0x0a, 0x06, 0x01, 0x04, 0x00, 0x41, 0x2a, 0x0b, // i32.const 42; end
};

} // namespace

TEST_CASE("a boot compiles the top species unless asked for -O0",
          "[tier-ladder]")
{
    unsetenv("RIVE_WASM_AOT_SYNC");
#ifdef RIVE_WASM_HW_BOUNDS
    CHECK(bootTierSpecies() == TierSpecies::hw);
#else
    CHECK(bootTierSpecies() == TierSpecies::o3);
#endif
    setenv("RIVE_WASM_AOT_SYNC", "o0", 1);
    CHECK(bootTierSpecies() == TierSpecies::o0);
    unsetenv("RIVE_WASM_AOT_SYNC");
}

// Hidden: needs a wamrc binary via RIVE_WAMRC, which CI and dev machines
// provide explicitly.
TEST_CASE("tier ladder compiles a module to an artifact", "[.][tier-ladder]")
{
    REQUIRE(getenv("RIVE_WAMRC") != nullptr);

    char dirTemplate[] = "/tmp/rive_tier_ladder_XXXXXX";
    char* cacheDir = mkdtemp(dirTemplate);
    REQUIRE(cacheDir != nullptr);

    auto& ladder = ModuleTierLadder::instance();
    ladder.configure(std::string(), cacheDir);
    REQUIRE(ladder.enabled());

    const TierSpecies kTopSpecies = bootTierSpecies();
    const uint64_t key = 0x1122334455667788ull;
    Span<const uint8_t> module(kTinyModule, sizeof(kTinyModule));
    CHECK(ladder.artifactPath(key, kTopSpecies).empty());
    TierCompile compile = ladder.compileSync(key, module, kTopSpecies);
    CHECK(compile.compiled);
    std::string path = compile.path;
    REQUIRE(!path.empty());
    CHECK(ladder.artifactPath(key, kTopSpecies) == path);
    CHECK(ladder.artifactPath(key, TierSpecies::o0).empty());

    std::ifstream artifact(path, std::ios::binary);
    char magic[4] = {0};
    artifact.read(magic, 4);
    CHECK(magic[0] == '\0');
    CHECK(magic[1] == 'a');
    CHECK(magic[2] == 'o');
    CHECK(magic[3] == 't');

    // A second compile of the same content is a cache hit.
    TierCompile hit = ladder.compileSync(key, module, kTopSpecies);
    CHECK(hit.path == path);
    CHECK(!hit.compiled);

    ladder.unconfigure();
}

#endif // WITH_RIVE_SCRIPTING_WASM
