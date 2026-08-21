#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/module_tier_ladder.hpp"
#include "rive/wasm/wamr_state_transplant.hpp"
#include "wasm_export.h"

#include <catch.hpp>

#include <cstdlib>
#include <unistd.h>
#include <fstream>
#include <vector>

using namespace rive;

namespace
{

// (module
//   (memory 1) (global $g (mut i32) (i32.const 0))
//   (func (export "bump") (result i32)  ;; ++g, mem[16]=g, returns g
//   (func (export "read") (result i32)) ;; mem[16] + g
static const uint8_t kStatefulModule[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60,
    0x00, 0x01, 0x7f, 0x03, 0x03, 0x02, 0x00, 0x00, 0x05, 0x03, 0x01, 0x00,
    0x01, 0x06, 0x06, 0x01, 0x7f, 0x01, 0x41, 0x00, 0x0b, 0x07, 0x0f, 0x02,
    0x04, 0x62, 0x75, 0x6d, 0x70, 0x00, 0x00, 0x04, 0x72, 0x65, 0x61, 0x64,
    0x00, 0x01, 0x0a, 0x1f, 0x02, 0x12, 0x00, 0x23, 0x00, 0x41, 0x01, 0x6a,
    0x24, 0x00, 0x41, 0x10, 0x23, 0x00, 0x36, 0x02, 0x00, 0x23, 0x00, 0x0b,
    0x0a, 0x00, 0x41, 0x10, 0x28, 0x02, 0x00, 0x23, 0x00, 0x6a, 0x0b,
};

struct Instance
{
    wasm_module_t module = nullptr;
    wasm_module_inst_t inst = nullptr;
    wasm_exec_env_t env = nullptr;
    std::vector<uint8_t> bytes;

    bool load(const uint8_t* data, size_t size)
    {
        bytes.assign(data, data + size);
        char error[128] = {0};
        module = wasm_runtime_load(bytes.data(),
                                   (uint32_t)bytes.size(),
                                   error,
                                   sizeof(error));
        if (module == nullptr)
        {
            UNSCOPED_INFO("load: " << error);
            return false;
        }
        inst = wasm_runtime_instantiate(module,
                                        64 * 1024,
                                        64 * 1024,
                                        error,
                                        sizeof(error));
        if (inst == nullptr)
        {
            UNSCOPED_INFO("instantiate: " << error);
            return false;
        }
        env = wasm_runtime_create_exec_env(inst, 64 * 1024);
        return env != nullptr;
    }

    uint32_t call(const char* name)
    {
        wasm_function_inst_t f = wasm_runtime_lookup_function(inst, name);
        REQUIRE(f != nullptr);
        uint32_t argv[1] = {0};
        REQUIRE(wasm_runtime_call_wasm(env, f, 0, argv));
        return argv[0];
    }

    ~Instance()
    {
        if (env != nullptr)
        {
            wasm_runtime_destroy_exec_env(env);
        }
        if (inst != nullptr)
        {
            wasm_runtime_deinstantiate(inst);
        }
        if (module != nullptr)
        {
            wasm_runtime_unload(module);
        }
    }
};

} // namespace

// Hidden: needs RIVE_WAMRC, and owns runtime init for the process (run the
// tag in isolation).
TEST_CASE("mid-run state survives an interp to aot transplant",
          "[.][tier-transplant]")
{
    REQUIRE(getenv("RIVE_WAMRC") != nullptr);
    REQUIRE(wasm_runtime_init());

    char dirTemplate[] = "/tmp/rive_transplant_XXXXXX";
    char* cacheDir = mkdtemp(dirTemplate);
    REQUIRE(cacheDir != nullptr);
    auto& ladder = ModuleTierLadder::instance();
    ladder.configure(std::string(), cacheDir);
    REQUIRE(ladder.enabled());

    const uint64_t key = 0xfeedfacecafef00dull;
    ladder.schedule(
        "transplant-lane",
        key,
        Span<const uint8_t>(kStatefulModule, sizeof(kStatefulModule)));
    ladder.drain();
    std::string aotPath = ladder.artifactPath(key, TierSpecies::o3);
    REQUIRE(!aotPath.empty());
    std::ifstream file(aotPath, std::ios::binary);
    std::vector<uint8_t> aotBytes(std::istreambuf_iterator<char>(file), {});
    REQUIRE(!aotBytes.empty());

    // Live source: three bumps on the interpreter.
    Instance interp;
    REQUIRE(interp.load(kStatefulModule, sizeof(kStatefulModule)));
    interp.call("bump");
    interp.call("bump");
    CHECK(interp.call("bump") == 3);

    // Fresh destination in the aot representation.
    Instance aot;
    REQUIRE(aot.load(aotBytes.data(), aotBytes.size()));
    CHECK(aot.call("read") == 0);

    std::string error;
    REQUIRE(wamrTransplantState(interp.inst, aot.inst, error));

    // The swap target continues exactly where the source stopped.
    CHECK(aot.call("read") == 6); // mem[16]=3 + g=3
    CHECK(aot.call("bump") == 4);
    CHECK(aot.call("bump") == 5);
    CHECK(aot.call("read") == 10);

    // Differential control: the same edit history on one engine.
    Instance control;
    REQUIRE(control.load(kStatefulModule, sizeof(kStatefulModule)));
    for (int i = 0; i < 5; i++)
    {
        control.call("bump");
    }
    CHECK(control.call("read") == aot.call("read"));
}

#endif // WITH_RIVE_SCRIPTING_WASM
