#include <rive/file.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>

// A ScriptedObject that is not a Component is not an artboard child, so
// ScriptInput*::import never hands its inputs to the ArtboardImporter and
// Artboard::m_Objects never frees them -- the object has to. Every such class
// implements that rule in disposeScriptInputs(); ScriptedInterpolator, added
// later, did not, so its inputs leaked from the moment a file carrying them
// was loaded.
//
// The assertion here is the sanitizer: LeakSanitizer in CI, `leaks` on macOS
// (`./test.sh memory`). This file's interpolator carries two ScriptInputs, so
// without the fix loading it alone leaks 320 bytes.
TEST_CASE("script inputs on a scripted interpolator are freed", "[scripting]")
{
    auto file = ReadRiveFile("assets/data_bound_keyframe_test.riv");
    REQUIRE(file != nullptr);
}
