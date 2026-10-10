#if defined(WITH_RIVE_SCRIPTING_WASM) && defined(WITH_RIVE_TOOLS)
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/wasm/wasm_scripting_vm.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

using namespace rive;

#ifndef __has_feature
#define __has_feature(x) 0
#endif

// animascript_input_keyboard.riv (source:
// assets/rml/animascript_input_keyboard/, unsigned, so WITH_RIVE_TOOLS): Main's
// ScriptedLayout runs an AnimaScript Layout whose `@input deck: Artboard?` is
// bound to the Deck artboard, and Deck's state machine has a keyboard listener
// on a shape carrying FocusData. The script's Deck instance lives behind a
// handle of the file's wasm VM, which sweeps it when the VM is destroyed. ~File
// used to destroy its wasm VMs only after deleting the artboards, so the Deck
// instance's KeyboardListenerGroup then read the freed StateMachineListener and
// FocusData (a SIGSEGV at every rive CLI teardown of such a file).
TEST_CASE("a file releases its wasm VMs before its artboards",
          "[scripting][wasm]")
{
    auto file = ReadRiveFile("assets/animascript_input_keyboard.riv");
    // ASan's larger frames overflow WAMR's native stack check at instantiate
    // (as in script_signature_test), so there the module never runs.
#if !defined(__SANITIZE_ADDRESS__) && !__has_feature(address_sanitizer)
    REQUIRE(file->wasmScriptingVM() != nullptr);
#endif
    {
        auto main = file->artboardNamed("Main");
        REQUIRE(main != nullptr);
        for (int frame = 0; frame < 3; frame++)
        {
            main->advance(1.0f / 60.0f);
        }
    }
    // The last reference: ~File must release the VM, and the Deck instance it
    // holds, before the artboards it was made from.
    file = nullptr;
    CHECK(file == nullptr);
}
#endif
