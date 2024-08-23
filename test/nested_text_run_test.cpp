#include "rive/core/binary_reader.hpp"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include <cstdio>

TEST_CASE("validate nested text get/set", "[nestedText]")
{
    auto file = ReadRiveFile("../../test/assets/runtime_nested_text_runs.riv");

    auto artboard = file->artboard("ArtboardA")->instance();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    // Test getting/setting TextValueRun view nested artboard path one level deep

    auto textRunB1 = artboard->getTextRun("ArtboardBRun", "ArtboardB-1");
    REQUIRE(textRunB1->is<rive::TextValueRun>());
    REQUIRE(textRunB1->text() == "Artboard B Run");

    auto textRunB2 = artboard->getTextRun("ArtboardBRun", "ArtboardB-2");
    REQUIRE(textRunB2->is<rive::TextValueRun>());
    REQUIRE(textRunB2->text() == "Artboard B Run");

    // Test getting/setting TextValueRun view nested artboard path two level deep

    auto textRunB1C1 = artboard->getTextRun("ArtboardCRun", "ArtboardB-1/ArtboardC-1");
    REQUIRE(textRunB1C1->is<rive::TextValueRun>());
    REQUIRE(textRunB1C1->text() == "Artboard C Run");

    auto textRunB1C2 = artboard->getTextRun("ArtboardCRun", "ArtboardB-1/ArtboardC-2");
    REQUIRE(textRunB1C2->is<rive::TextValueRun>());
    REQUIRE(textRunB1C2->text() == "Artboard C Run");

    auto textRunB2C1 = artboard->getTextRun("ArtboardCRun", "ArtboardB-2/ArtboardC-1");
    REQUIRE(textRunB2C1->is<rive::TextValueRun>());
    REQUIRE(textRunB2C1->text() == "Artboard C Run");

    auto textRunB2C2 = artboard->getTextRun("ArtboardCRun", "ArtboardB-2/ArtboardC-2");
    REQUIRE(textRunB2C2->is<rive::TextValueRun>());
    REQUIRE(textRunB2C2->text() == "Artboard C Run");

    // Validate that text run values can be set

    textRunB1->text("Artboard B1 Run Updated");
    textRunB2->text("Artboard B2 Run Updated");
    textRunB1C1->text("Artboard B1C1 Run Updated");
    textRunB1C2->text("Artboard B1C2 Run Updated");
    textRunB2C1->text("Artboard B2C1 Run Updated");
    textRunB2C2->text("Artboard B2C2 Run Updated");
    REQUIRE(textRunB1->text() == "Artboard B1 Run Updated");
    REQUIRE(textRunB2->text() == "Artboard B2 Run Updated");
    REQUIRE(textRunB1C1->text() == "Artboard B1C1 Run Updated");
    REQUIRE(textRunB1C2->text() == "Artboard B1C2 Run Updated");
    REQUIRE(textRunB2C1->text() == "Artboard B2C1 Run Updated");
    REQUIRE(textRunB2C2->text() == "Artboard B2C2 Run Updated");
}