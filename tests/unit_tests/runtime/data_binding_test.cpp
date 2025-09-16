#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_renderer.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/color.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/text/text_value_run.hpp>
#include <rive/custom_property_boolean.hpp>
#include <rive/custom_property_number.hpp>
#include <rive/custom_property_string.hpp>
#include <rive/custom_property_trigger.hpp>
#include <rive/constraints/follow_path_constraint.hpp>
#include <rive/viewmodel/runtime/viewmodel_runtime.hpp>
#include <rive/viewmodel/runtime/viewmodel_instance_runtime.hpp>
#include <rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp>
#include <rive/data_bind/data_values/data_type.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_enum.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_list_item.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "rive/nested_artboard.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("artboard with bound properties", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test.riv");

    auto artboard = file->artboard("artboard-1")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);
    REQUIRE(artboard->find<rive::Rectangle>("bound_rect") != nullptr);
    auto rectMapped = artboard->find<rive::Rectangle>("bound_rect");
    REQUIRE(rectMapped->width() == 100.0f);
    REQUIRE(artboard->find<rive::Shape>("bound_rect_shape") != nullptr);
    auto shapeMapped = artboard->find<rive::Shape>("bound_rect_shape");
    // Rotation has a system converter applied to it
    REQUIRE(shapeMapped->rotation() == Approx(1.5708f));

    REQUIRE(shapeMapped->children()[1]->is<rive::Fill>());
    rive::Fill* rectMappadFill = shapeMapped->children()[1]->as<rive::Fill>();
    REQUIRE(rectMappadFill->paint()->is<rive::SolidColor>());
    REQUIRE(rectMappadFill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 255, 0, 0));

    REQUIRE(artboard->find<rive::TextValueRun>("bound_text_run") != nullptr);
    auto textRunMapped = artboard->find<rive::TextValueRun>("bound_text_run");
    REQUIRE(textRunMapped->text() == "bound text");

    REQUIRE(artboard->find<rive::FollowPathConstraint>("") != nullptr);
    auto followPathConstraint = artboard->find<rive::FollowPathConstraint>("");
    REQUIRE(followPathConstraint->orient() == false);

    // View model properties
    // Number
    auto widthProperty = viewModelInstance->propertyValue("width");
    REQUIRE(widthProperty != nullptr);
    REQUIRE(widthProperty->is<rive::ViewModelInstanceNumber>());
    // Number with comverter
    auto rotationProperty = viewModelInstance->propertyValue("rotation");
    REQUIRE(rotationProperty != nullptr);
    REQUIRE(rotationProperty->is<rive::ViewModelInstanceNumber>());
    // Color
    auto colorProperty = viewModelInstance->propertyValue("color");
    REQUIRE(colorProperty != nullptr);
    REQUIRE(colorProperty->is<rive::ViewModelInstanceColor>());
    // String
    auto textProperty = viewModelInstance->propertyValue("text");
    REQUIRE(textProperty != nullptr);
    REQUIRE(textProperty->is<rive::ViewModelInstanceString>());
    // Boolean
    auto orientProperty = viewModelInstance->propertyValue("orient");
    REQUIRE(orientProperty != nullptr);
    REQUIRE(orientProperty->is<rive::ViewModelInstanceBoolean>());
    // Update view model values
    widthProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(200.0f);
    rotationProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(
        180.0f);
    colorProperty->as<rive::ViewModelInstanceColor>()->propertyValue(
        rive::colorARGB(255, 0, 255, 0));
    textProperty->as<rive::ViewModelInstanceString>()->propertyValue(
        "New text");
    orientProperty->as<rive::ViewModelInstanceBoolean>()->propertyValue(true);
    // Advance artboard so data binds apply
    artboard->advance(0.0f);
    // Validate new properties
    REQUIRE(rectMapped->width() == 200.0f);
    REQUIRE(shapeMapped->rotation() == Approx(3.14159f));
    REQUIRE(rectMappadFill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 0, 255, 0));
    REQUIRE(textRunMapped->text() == "New text");
    REQUIRE(followPathConstraint->orient() == true);
}

TEST_CASE("state machine led by enums and triggers", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test.riv");

    auto artboard = file->artboard("artboard-2")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    //

    REQUIRE(artboard->find<rive::Shape>("color_rectangle") != nullptr);
    auto shapeMapped = artboard->find<rive::Shape>("color_rectangle");

    REQUIRE(shapeMapped->children()[1]->is<rive::Fill>());
    REQUIRE(shapeMapped->x() == 250);
    REQUIRE(shapeMapped->y() == 250);
    rive::Fill* rectMappadFill = shapeMapped->children()[1]->as<rive::Fill>();
    REQUIRE(rectMappadFill->paint()->is<rive::SolidColor>());
    REQUIRE(rectMappadFill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 116, 116, 116));

    // View model properties
    // Enum
    auto stateProperty = viewModelInstance->propertyValue("state");
    REQUIRE(stateProperty != nullptr);
    REQUIRE(stateProperty->is<rive::ViewModelInstanceEnum>());
    // Trigger
    auto triggerProperty = viewModelInstance->propertyValue("trigger-prop");
    REQUIRE(triggerProperty != nullptr);
    REQUIRE(triggerProperty->is<rive::ViewModelInstanceTrigger>());
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(rectMappadFill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 255, 0, 0));
    // Update view model properties
    // Update enum by index
    stateProperty->as<rive::ViewModelInstanceEnum>()->value(1);

    // Advance state machine
    machine->advanceAndApply(0.0f);
    // Validate values have updated
    REQUIRE(rectMappadFill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 0, 255, 0));
    REQUIRE(shapeMapped->x() == 150);
    REQUIRE(shapeMapped->y() == 250);
    // Update view model properties
    // Update enum by name
    stateProperty->as<rive::ViewModelInstanceEnum>()->value("state-blue");
    // Update trigger
    triggerProperty->as<rive::ViewModelInstanceTrigger>()->propertyValue(1);

    // Advance state machine
    machine->advanceAndApply(0.0f);
    // Validate values have updated
    REQUIRE(rectMappadFill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 0, 0, 255));
    REQUIRE(shapeMapped->x() == 350);
    REQUIRE(shapeMapped->y() == 250);
    // Update trigger
    triggerProperty->as<rive::ViewModelInstanceTrigger>()->propertyValue(1);

    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(shapeMapped->x() == 350);
    REQUIRE(shapeMapped->y() == 350);
}

TEST_CASE("calculate and to string converters with numbers", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test.riv");

    auto artboard = file->artboard("artboard-3")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    // Bound property with Calculate Converter (multiply by 2)
    REQUIRE(artboard->find<rive::CustomPropertyNumber>("num_prop") != nullptr);
    auto customPropertyNumber =
        artboard->find<rive::CustomPropertyNumber>("num_prop");
    REQUIRE(customPropertyNumber->propertyValue() == 0.0f);
    // Bound property with Group Converter with:
    // - Calculate (divide by 3)
    // - Convert to string (round decimals and remove trailing zeros)
    REQUIRE(artboard->find<rive::TextValueRun>("text_run_bound") != nullptr);
    auto textRunBound = artboard->find<rive::TextValueRun>("text_run_bound");
    // View model properties
    // Number with initial value set to 17
    auto numProperty = viewModelInstance->propertyValue("num1");
    REQUIRE(numProperty != nullptr);
    REQUIRE(numProperty->is<rive::ViewModelInstanceNumber>());

    REQUIRE(textRunBound->text() == "text");
    // Advance state machine
    machine->advanceAndApply(0.0f);
    // Test Calculate Converter (multiply by 2)
    REQUIRE(customPropertyNumber->propertyValue() == 34.0f);
    REQUIRE(textRunBound->text() == "6");

    // Update value to -10.0f
    numProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(-10.0f);
    // Advance state machine
    machine->advanceAndApply(0.0f);
    // Test Calculate Converter (multiply by 2)
    REQUIRE(customPropertyNumber->propertyValue() == -20.0f);
    REQUIRE(textRunBound->text() == "-3");
}

TEST_CASE("trim string converter", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test.riv");

    auto artboard = file->artboard("artboard-3")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);

    REQUIRE(artboard->find<rive::TextValueRun>("second_text_run_trim_both") !=
            nullptr);
    auto trimmedBothTextRunBound =
        artboard->find<rive::TextValueRun>("second_text_run_trim_both");

    REQUIRE(artboard->find<rive::TextValueRun>("second_text_run_trim_start") !=
            nullptr);
    auto trimmedStartTextRunBound =
        artboard->find<rive::TextValueRun>("second_text_run_trim_start");

    REQUIRE(artboard->find<rive::TextValueRun>("second_text_run_trim_end") !=
            nullptr);
    auto trimmedEndTextRunBound =
        artboard->find<rive::TextValueRun>("second_text_run_trim_end");

    REQUIRE(artboard->find<rive::TextValueRun>("second_text_run_no_trim") !=
            nullptr);
    auto notTrimmedTextRunBound =
        artboard->find<rive::TextValueRun>("second_text_run_no_trim");
    // View model properties
    // String with initial value "     abc    "
    auto stringProperty = viewModelInstance->propertyValue("text");
    REQUIRE(stringProperty != nullptr);
    REQUIRE(stringProperty->is<rive::ViewModelInstanceString>());

    REQUIRE(notTrimmedTextRunBound->text() == "text");
    REQUIRE(trimmedBothTextRunBound->text() == "text");
    REQUIRE(trimmedStartTextRunBound->text() == "text");
    REQUIRE(trimmedEndTextRunBound->text() == "text");
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(trimmedBothTextRunBound->text() == "abc");
    REQUIRE(notTrimmedTextRunBound->text() == "     abc    ");
    REQUIRE(trimmedStartTextRunBound->text() == "abc    ");
    REQUIRE(trimmedEndTextRunBound->text() == "     abc");

    // Update value to "a b c "
    stringProperty->as<rive::ViewModelInstanceString>()->propertyValue(
        "a b c ");
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(notTrimmedTextRunBound->text() == "a b c ");
    REQUIRE(trimmedBothTextRunBound->text() == "a b c");
    REQUIRE(trimmedStartTextRunBound->text() == "a b c ");
    REQUIRE(trimmedEndTextRunBound->text() == "a b c");
}

TEST_CASE("To string converter with color formatters", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test.riv");

    auto artboard = file->artboard("artboard-4")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);

    REQUIRE(artboard->find<rive::TextValueRun>("RGBA_formatted_color_run") !=
            nullptr);
    auto RGBATextRunBound =
        artboard->find<rive::TextValueRun>("RGBA_formatted_color_run");

    REQUIRE(artboard->find<rive::TextValueRun>("rgba_formatted_color_run") !=
            nullptr);
    auto rgbaTextRunBound =
        artboard->find<rive::TextValueRun>("rgba_formatted_color_run");

    REQUIRE(artboard->find<rive::TextValueRun>("hls_formatted_color_run") !=
            nullptr);
    auto hlsTextRunBound =
        artboard->find<rive::TextValueRun>("hls_formatted_color_run");

    REQUIRE(artboard->find<rive::TextValueRun>("escaped_characters_run") !=
            nullptr);
    auto escapedTextRunBound =
        artboard->find<rive::TextValueRun>("escaped_characters_run");

    // View model properties
    // color with initial value "red 30, green 90, blue 200, alpha 255"
    auto colorProperty = viewModelInstance->propertyValue("col");
    REQUIRE(colorProperty != nullptr);
    REQUIRE(colorProperty->is<rive::ViewModelInstanceColor>());

    REQUIRE(RGBATextRunBound->text() == "text");
    REQUIRE(rgbaTextRunBound->text() == "text");
    REQUIRE(hlsTextRunBound->text() == "text");
    REQUIRE(escapedTextRunBound->text() == "text");
    // // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(RGBATextRunBound->text() ==
            "color: {red: 1E, green: 5A, blue: C8, alpha: FF}");
    REQUIRE(rgbaTextRunBound->text() ==
            "color: {red: 30, green: 90, blue: 200, alpha: 255}");
    REQUIRE(hlsTextRunBound->text() ==
            "color: {hue: 219, luminance: 45, saturation: 74}");
    REQUIRE(escapedTextRunBound->text() == "%r %g %b %a \\a");

    // // Update value to "red 200, green 100, blue 50, alpha 100"
    colorProperty->as<rive::ViewModelInstanceColor>()->propertyValue(
        rive::colorARGB(100, 200, 100, 50));
    // // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(RGBATextRunBound->text() ==
            "color: {red: C8, green: 64, blue: 32, alpha: 64}");
    REQUIRE(rgbaTextRunBound->text() ==
            "color: {red: 200, green: 100, blue: 50, alpha: 100}");
    REQUIRE(hlsTextRunBound->text() ==
            "color: {hue: 20, luminance: 49, saturation: 60}");
    REQUIRE(escapedTextRunBound->text() == "%r %g %b %a \\a");

    // // Update value to "red 0, green 10, blue 16, alpha 100"
    colorProperty->as<rive::ViewModelInstanceColor>()->propertyValue(
        rive::colorARGB(100, 0, 10, 15));
    // // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(RGBATextRunBound->text() ==
            "color: {red: 00, green: 0A, blue: 0F, alpha: 64}");
    REQUIRE(rgbaTextRunBound->text() ==
            "color: {red: 0, green: 10, blue: 15, alpha: 100}");
    REQUIRE(hlsTextRunBound->text() ==
            "color: {hue: 200, luminance: 3, saturation: 100}");
    REQUIRE(escapedTextRunBound->text() == "%r %g %b %a \\a");
}

struct FormulaResult
{
    std::string runName;
    std::string result;

    // Constructor
    FormulaResult(const std::string& name, const std::string& res) :
        runName(name), result(res)
    {}
};

// TEST_CASE("Formula converter", "[data binding]")
// {
//     auto file = ReadRiveFile("assets/data_binding_test.riv");

//     auto artboard = file->artboard("artboard-5")->instance();
//     REQUIRE(artboard != nullptr);
//     auto viewModelInstance =
//         file->createDefaultViewModelInstance(artboard.get());
//     REQUIRE(viewModelInstance != nullptr);
//     auto machine = artboard->defaultStateMachine();
//     machine->bindViewModelInstance(viewModelInstance);
//     REQUIRE(machine != nullptr);
//     // Advance state machine
//     machine->advanceAndApply(0.0f);
//     // Initial input is 1.1
//     std::vector<FormulaResult> results = {
//         {"result-run-1", "11.1"},      // {Input} + 10.0
//         {"result-run-2", "0.11"},      // {Input} / 10.0
//         {"result-run-3", "-8.9"},      // {Input} - 10.0
//         {"result-run-4", "11"},        // {Input} * 10.0
//         {"result-run-5", "0.2"},       // {Input} % 0.9
//         {"result-run-6", "0.891207"},  // sin({Input})
//         {"result-run-7", "0.453596"},  // cos({Input})
//         {"result-run-8", "1.96476"},   // tan({Input})
//         {"result-run-9", "1.460573"},  // acos({Input}/10.0)
//         {"result-run-10", "0.110223"}, // asin({Input}/10.0)
//         {"result-run-11", "0.10956"},  // atan({Input}/10.0)
//         {"result-run-12", "0.03665"},  // atan2({Input}/10.0,3.0)
//         {"result-run-13", "1"},        // max(-1.0,{Input}/10.0,1.0)
//         {"result-run-14", "-1.1"},     // min(-1.0,-1.0*{Input},1.0)
//         {"result-run-15", "1"},        // round({Input})
//         {"result-run-16", "2"},        // ceil({Input})
//         {"result-run-17", "1"},        // floor({Input})
//         {"result-run-18", "1.048809"}, // sqrt({Input})
//         {"result-run-19", "1.21"},     // pow({Input},2.0)
//         {"result-run-20", "3.004166"}, // exp({Input})
//         {"result-run-21", "0.09531"},  // log({Input})
//         {"result-run-22",
//          "1.546404"}, // max(-1.0*cos({Input})+min(2.0,5.0),-3.0)
//         {"result-run-23",
//          "1"}, // max(min(max(floor({Input}),-10.0),10.0),-10.0)
//         {"result-run-24",
//          "-9565.368164"},          //
//          cos(sin(tan({Input})))+exp(10.0)/log(0.1)
//         {"result-run-25", "1.11"}, // {Input}*{Input}+{Input}/{Input}-{Input}
//         {"result-run-26",
//          "1.557408"}, // (sin(1.0)/cos(1.0))*(((exp(log(1.0)))))
//         {"result-run-27", "0.910958"},  // pow(sin(1.0),cos(1.0))
//         {"result-run-28", "0.1"},       // {Input}%round({Input})
//         {"result-run-29", "14.993111"},
//         // 4.0/{Input}*sqrt(17.0+max(-2.0,0.0))
//         {"result-run-30", "1"}, //
//         ceil(max({Input}*2.0/10.0,{Input}/10.0*2.0))

//     };
//     for (auto& formulaResult : results)
//     {
//         REQUIRE(artboard->find<rive::TextValueRun>(formulaResult.runName) !=
//                 nullptr);
//         auto formulaTextRun =
//             artboard->find<rive::TextValueRun>(formulaResult.runName);
//         REQUIRE(formulaTextRun->text() == formulaResult.result);
//     }
// }

// TODO: revisit this test when data binding in design mode is merged
// TEST_CASE("Time based interpolator", "[data binding]")
// {
//     auto file = ReadRiveFile("assets/data_binding_test_2.riv");

//     auto artboard = file->artboard("artboard-1")->instance();
//     REQUIRE(artboard != nullptr);
//     auto viewModelInstance =
//         file->createDefaultViewModelInstance(artboard.get());
//     REQUIRE(viewModelInstance != nullptr);
//     auto machine = artboard->defaultStateMachine();
//     machine->bindViewModelInstance(viewModelInstance);
//     REQUIRE(machine != nullptr);
//     // Advance state machine
//     // artboard->advance(0);
//     machine->advanceAndApply(0.0f);

//     REQUIRE(artboard->find<rive::Shape>("rect") != nullptr);
//     auto shapeRect = artboard->find<rive::Shape>("rect");
//     REQUIRE(shapeRect->x() == 0);
//     // View model properties
//     // Number
//     auto numProperty = viewModelInstance->propertyValue("num");
//     REQUIRE(numProperty != nullptr);
//     REQUIRE(numProperty->is<rive::ViewModelInstanceNumber>());
//     numProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(100.0f);
//     fprintf(stderr, "==> machine advance\n");
//     machine->advanceAndApply(0.0f);
//     // Advance state machine
//     fprintf(stderr, "==> machine advance\n");
//     machine->advanceAndApply(0.25f);
//     fprintf(stderr, "==> machine advance\n");
//     machine->advanceAndApply(0.25f);
//     fprintf(stderr, "shapeRect->x(): %f\n", shapeRect->x());
//     REQUIRE(shapeRect->x() == 25.0f);
//     machine->advanceAndApply(0.25f);
//     fprintf(stderr, "shapeRect->x(): %f\n", shapeRect->x());
//     REQUIRE(shapeRect->x() == 50.0f);
// }

TEST_CASE("Range Mapper", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_2.riv");

    auto artboard = file->artboard("artboard-2")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    // Advance state machine
    machine->advanceAndApply(0.0f);

    // Range mapper [2 - 3]
    REQUIRE(artboard->find<rive::CustomPropertyNumber>("mapped-range-1") !=
            nullptr);
    auto customPropertyNumber1 =
        artboard->find<rive::CustomPropertyNumber>("mapped-range-1");
    REQUIRE(customPropertyNumber1->propertyValue() == 6.0f);

    // Range mapper [2 - 3] - lower clamp - upper clamp
    REQUIRE(artboard->find<rive::CustomPropertyNumber>("mapped-range-2") !=
            nullptr);
    auto customPropertyNumber2 =
        artboard->find<rive::CustomPropertyNumber>("mapped-range-2");
    REQUIRE(customPropertyNumber2->propertyValue() == 3.0f);

    // Range mapper [2 - 3] - modulo
    REQUIRE(artboard->find<rive::CustomPropertyNumber>("mapped-range-3") !=
            nullptr);
    auto customPropertyNumber3 =
        artboard->find<rive::CustomPropertyNumber>("mapped-range-3");
    REQUIRE(customPropertyNumber3->propertyValue() == 2.0f);

    // Range mapper [2 - 3] - lower clamp - upper clamp - reversed
    REQUIRE(artboard->find<rive::CustomPropertyNumber>("mapped-range-4") !=
            nullptr);
    auto customPropertyNumber4 =
        artboard->find<rive::CustomPropertyNumber>("mapped-range-4");
    REQUIRE(customPropertyNumber4->propertyValue() == 2.0f);

    // Range mapper [2 - 2]
    REQUIRE(artboard->find<rive::CustomPropertyNumber>("mapped-range-5") !=
            nullptr);
    auto customPropertyNumber5 =
        artboard->find<rive::CustomPropertyNumber>("mapped-range-5");
    REQUIRE(customPropertyNumber5->propertyValue() == 2.0f);

    // View model properties
    // Number starts at 4.0f
    auto numProperty = viewModelInstance->propertyValue("map-range-num");
    REQUIRE(numProperty != nullptr);
    REQUIRE(numProperty->is<rive::ViewModelInstanceNumber>());
    REQUIRE(numProperty->as<rive::ViewModelInstanceNumber>()->propertyValue() ==
            4.0f);

    // Change value to -1.0f
    numProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(-1.0f);
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(customPropertyNumber1->propertyValue() == 1.0f);
    REQUIRE(customPropertyNumber2->propertyValue() == 2.0f);
    REQUIRE(customPropertyNumber3->propertyValue() == 2.0f);
    REQUIRE(customPropertyNumber4->propertyValue() == 3.0f);
    REQUIRE(customPropertyNumber5->propertyValue() == 2.0f);

    // Change value to 0.0f
    numProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(0.0f);
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(customPropertyNumber1->propertyValue() == 2.0f);
    REQUIRE(customPropertyNumber2->propertyValue() == 2.0f);
    REQUIRE(customPropertyNumber3->propertyValue() == 2.0f);
    REQUIRE(customPropertyNumber4->propertyValue() == 3.0f);
    REQUIRE(customPropertyNumber5->propertyValue() == 2.0f);

    // Change value to 0.25f
    numProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(0.25f);
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(customPropertyNumber1->propertyValue() == Approx(2.12916f));
    REQUIRE(customPropertyNumber2->propertyValue() == Approx(2.12916f));
    REQUIRE(customPropertyNumber3->propertyValue() == Approx(2.12916f));
    REQUIRE(customPropertyNumber4->propertyValue() == Approx(2.87084f));
    REQUIRE(customPropertyNumber5->propertyValue() == 2.0f);

    // Change value to 2.0f
    numProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(2.0f);
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(customPropertyNumber1->propertyValue() == 4.0f);
    REQUIRE(customPropertyNumber2->propertyValue() == 3.0f);
    REQUIRE(customPropertyNumber3->propertyValue() == 2.0f);
    REQUIRE(customPropertyNumber4->propertyValue() == 2.0f);
    REQUIRE(customPropertyNumber5->propertyValue() == 2.0f);

    // Change value to 2.25f
    numProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(2.25f);
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(customPropertyNumber1->propertyValue() == 4.25f);
    REQUIRE(customPropertyNumber2->propertyValue() == 3.0f);
    REQUIRE(customPropertyNumber3->propertyValue() == Approx(2.12916f));
    REQUIRE(customPropertyNumber4->propertyValue() == 2.0f);
    REQUIRE(customPropertyNumber5->propertyValue() == 2.0f);
}

TEST_CASE("Pad String", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_2.riv");

    auto artboard = file->artboard("artboard-3")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    // Advance state machine
    machine->advanceAndApply(0.0f);

    // Pad string "abc" - length: 11 - Start
    REQUIRE(artboard->find<rive::CustomPropertyString>("pad-string-1") !=
            nullptr);
    auto customPropertyString1 =
        artboard->find<rive::CustomPropertyString>("pad-string-1");
    REQUIRE(customPropertyString1->propertyValue() == "abcabcatext");

    // Pad string "abc" - length: 12 - End
    REQUIRE(artboard->find<rive::CustomPropertyString>("pad-string-2") !=
            nullptr);
    auto customPropertyString2 =
        artboard->find<rive::CustomPropertyString>("pad-string-2");
    REQUIRE(customPropertyString2->propertyValue() == "textabcabcab");

    // Pad string "abc" - length: 12 - End - Worng type of input
    REQUIRE(artboard->find<rive::CustomPropertyString>("pad-string-3") !=
            nullptr);
    auto customPropertyString3 =
        artboard->find<rive::CustomPropertyString>("pad-string-3");
    REQUIRE(customPropertyString3->propertyValue() == "");

    // View model properties
    // String starts with "text"
    auto stringProperty = viewModelInstance->propertyValue("pad-string");
    REQUIRE(stringProperty != nullptr);
    REQUIRE(stringProperty->is<rive::ViewModelInstanceString>());
    REQUIRE(
        stringProperty->as<rive::ViewModelInstanceString>()->propertyValue() ==
        "text");

    // Change value to "text-text-text", longer than length
    stringProperty->as<rive::ViewModelInstanceString>()->propertyValue(
        "text-text-text");
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(customPropertyString1->propertyValue() == "text-text-text");
    REQUIRE(customPropertyString2->propertyValue() == "text-text-text");
    REQUIRE(customPropertyString3->propertyValue() == "");

    // Change value to "", empty string
    stringProperty->as<rive::ViewModelInstanceString>()->propertyValue("");
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(customPropertyString1->propertyValue() == "abcabcabcab");
    REQUIRE(customPropertyString2->propertyValue() == "abcabcabcabc");
    REQUIRE(customPropertyString3->propertyValue() == "");
}

TEST_CASE("Boolean Toggle", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_2.riv");

    auto artboard = file->artboard("artboard-3")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    // Advance state machine
    machine->advanceAndApply(0.0f);

    // Boolean property
    REQUIRE(artboard->find<rive::CustomPropertyBoolean>("negate-bool-1") !=
            nullptr);
    auto customPropertyBoolean1 =
        artboard->find<rive::CustomPropertyBoolean>("negate-bool-1");
    REQUIRE(customPropertyBoolean1->propertyValue() == true);

    // View model properties
    // bool property starts as false
    auto boolProperty = viewModelInstance->propertyValue("bool-prop");
    REQUIRE(boolProperty != nullptr);
    REQUIRE(boolProperty->is<rive::ViewModelInstanceBoolean>());
    REQUIRE(
        boolProperty->as<rive::ViewModelInstanceBoolean>()->propertyValue() ==
        false);

    // Change value to true
    boolProperty->as<rive::ViewModelInstanceBoolean>()->propertyValue(true);
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(customPropertyBoolean1->propertyValue() == false);
}

TEST_CASE("Instance is shared when the same one is attached to two properties",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/shared_viewmodel_instance.riv");

    auto artboard = file->artboard("main")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    // Advance state machine
    machine->advanceAndApply(0.0f);

    // View model properties
    auto childInstanceViewModel1 = viewModelInstance->propertyValue("child1");
    REQUIRE(childInstanceViewModel1 != nullptr);
    REQUIRE(childInstanceViewModel1->is<rive::ViewModelInstanceViewModel>());
    auto referencedViewModel =
        childInstanceViewModel1->as<rive::ViewModelInstanceViewModel>()
            ->referenceViewModelInstance();
    REQUIRE(referencedViewModel != nullptr);
    auto labelProperty = referencedViewModel->propertyValue("label");
    REQUIRE(labelProperty != nullptr);
    REQUIRE(labelProperty->is<rive::ViewModelInstanceString>());

    // Elements bound to different view model properties that share same view
    // model instance
    REQUIRE(artboard->find<rive::NestedArtboard>("child1") != nullptr);
    auto nestedArtboardChild1 = artboard->find<rive::NestedArtboard>("child1");

    auto nestedArtboardArtboardChild1 =
        nestedArtboardChild1->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild1 != nullptr);
    auto textRunChild1 =
        nestedArtboardArtboardChild1->find<rive::TextValueRun>("text_run");
    REQUIRE(textRunChild1 != nullptr);
    REQUIRE(textRunChild1->text() == "label-vmi-1");

    REQUIRE(artboard->find<rive::NestedArtboard>("child2") != nullptr);
    auto nestedArtboardChild2 = artboard->find<rive::NestedArtboard>("child2");

    auto nestedArtboardArtboardChild2 =
        nestedArtboardChild2->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild2 != nullptr);
    auto textRunChild2 =
        nestedArtboardArtboardChild2->find<rive::TextValueRun>("text_run");
    REQUIRE(textRunChild2 != nullptr);
    REQUIRE(textRunChild2->text() == "label-vmi-1");

    // Changing the value on a single instance should affect both text although
    // they are linked to different view model properties
    labelProperty->as<rive::ViewModelInstanceString>()->propertyValue(
        "label-update");
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(textRunChild1->text() == "label-update");
    REQUIRE(textRunChild2->text() == "label-update");
}

TEST_CASE("Instance is not shared when the different ones are attached to two "
          "properties",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/shared_viewmodel_instance.riv");

    auto artboard = file->artboard("main_2")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    // Advance state machine
    machine->advanceAndApply(0.0f);

    // View model properties
    auto childInstanceViewModel1 =
        viewModelInstance->propertyValue("vm_2_child1");
    REQUIRE(childInstanceViewModel1 != nullptr);
    REQUIRE(childInstanceViewModel1->is<rive::ViewModelInstanceViewModel>());
    auto referencedViewModel =
        childInstanceViewModel1->as<rive::ViewModelInstanceViewModel>()
            ->referenceViewModelInstance();
    REQUIRE(referencedViewModel != nullptr);
    auto labelProperty = referencedViewModel->propertyValue("label");
    REQUIRE(labelProperty != nullptr);
    REQUIRE(labelProperty->is<rive::ViewModelInstanceString>());

    // Elements bound to different view model properties that do not share same
    // view model instance
    REQUIRE(artboard->find<rive::NestedArtboard>("child1") != nullptr);
    auto nestedArtboardChild1 = artboard->find<rive::NestedArtboard>("child1");

    auto nestedArtboardArtboardChild1 =
        nestedArtboardChild1->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild1 != nullptr);
    auto textRunChild1 =
        nestedArtboardArtboardChild1->find<rive::TextValueRun>("text_run");
    REQUIRE(textRunChild1 != nullptr);
    REQUIRE(textRunChild1->text() == "label-vmi-1");

    REQUIRE(artboard->find<rive::NestedArtboard>("child2") != nullptr);
    auto nestedArtboardChild2 = artboard->find<rive::NestedArtboard>("child2");

    auto nestedArtboardArtboardChild2 =
        nestedArtboardChild2->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild2 != nullptr);
    auto textRunChild2 =
        nestedArtboardArtboardChild2->find<rive::TextValueRun>("text_run");
    REQUIRE(textRunChild2 != nullptr);
    REQUIRE(textRunChild2->text() == "label-vmi-2");

    // Changing the value on a single instance should not affect the other
    // instance because they are not pointing to the same instance
    labelProperty->as<rive::ViewModelInstanceString>()->propertyValue(
        "label-update");
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(textRunChild1->text() == "label-update");
    REQUIRE(textRunChild2->text() == "label-vmi-2");
}

TEST_CASE("Instances are not shared when a new view model instance is created",
          "[data binding]")
{
    auto file = ReadRiveFile("assets/shared_viewmodel_instance.riv");

    auto artboard = file->artboard("main_2")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance = file->createViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    // Advance state machine
    machine->advanceAndApply(0.0f);

    // View model properties
    auto childInstanceViewModel1 =
        viewModelInstance->propertyValue("vm_2_child1");
    REQUIRE(childInstanceViewModel1 != nullptr);
    REQUIRE(childInstanceViewModel1->is<rive::ViewModelInstanceViewModel>());
    auto referencedViewModel =
        childInstanceViewModel1->as<rive::ViewModelInstanceViewModel>()
            ->referenceViewModelInstance();
    REQUIRE(referencedViewModel != nullptr);
    auto labelProperty = referencedViewModel->propertyValue("label");
    REQUIRE(labelProperty != nullptr);
    REQUIRE(labelProperty->is<rive::ViewModelInstanceString>());

    // Elements bound to different view model properties that do not share same
    // view model instance
    REQUIRE(artboard->find<rive::NestedArtboard>("child1") != nullptr);
    auto nestedArtboardChild1 = artboard->find<rive::NestedArtboard>("child1");

    auto nestedArtboardArtboardChild1 =
        nestedArtboardChild1->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild1 != nullptr);
    auto textRunChild1 =
        nestedArtboardArtboardChild1->find<rive::TextValueRun>("text_run");
    REQUIRE(textRunChild1 != nullptr);
    REQUIRE(textRunChild1->text() == "");

    REQUIRE(artboard->find<rive::NestedArtboard>("child2") != nullptr);
    auto nestedArtboardChild2 = artboard->find<rive::NestedArtboard>("child2");

    auto nestedArtboardArtboardChild2 =
        nestedArtboardChild2->artboardInstance();
    REQUIRE(nestedArtboardArtboardChild2 != nullptr);
    auto textRunChild2 =
        nestedArtboardArtboardChild2->find<rive::TextValueRun>("text_run");
    REQUIRE(textRunChild2 != nullptr);
    REQUIRE(textRunChild2->text() == "");

    // Changing the value on a single instance should not affect the other
    // instance because they are not pointing to the same instance
    labelProperty->as<rive::ViewModelInstanceString>()->propertyValue(
        "label-update");
    // Advance state machine
    machine->advanceAndApply(0.0f);
    REQUIRE(textRunChild1->text() == "label-update");
    REQUIRE(textRunChild2->text() == "");
}

TEST_CASE("Triggers updated by events correctly update state", "[data binding]")
{
    auto file = ReadRiveFile("assets/data_binding_test_triggers.riv");

    auto artboard = file->artboard("root")->instance();
    REQUIRE(artboard != nullptr);
    auto viewModelInstance = file->createViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    // Advance state machine
    machine->advanceAndApply(0.0f);

    REQUIRE(artboard->find<rive::Shape>("main_rect") != nullptr);
    auto rect = artboard->find<rive::Shape>("main_rect");

    REQUIRE(rect->children()[1]->is<rive::Fill>());
    rive::Fill* rectMappadFill = rect->children()[1]->as<rive::Fill>();
    REQUIRE(rectMappadFill->paint()->is<rive::SolidColor>());
    REQUIRE(rectMappadFill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 255, 0, 0));

    // Advance state machine so the child reports the event
    machine->advanceAndApply(0.7f);
    // Advance state machine so the parent consumes the event
    machine->advanceAndApply(0.1f);
    REQUIRE(rectMappadFill->paint()->as<rive::SolidColor>()->colorValue() ==
            rive::colorARGB(255, 0, 255, 0));
}

TEST_CASE("Transition self conditions", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/transition_self_comparator_test.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    auto numProp =
        vmi->propertyValue("num")->as<rive::ViewModelInstanceNumber>();
    auto triggerProp =
        vmi->propertyValue("tri")->as<rive::ViewModelInstanceTrigger>();
    auto colorProp =
        vmi->propertyValue("col")->as<rive::ViewModelInstanceColor>();
    auto bolProp =
        vmi->propertyValue("bol")->as<rive::ViewModelInstanceBoolean>();
    auto stringProp =
        vmi->propertyValue("str")->as<rive::ViewModelInstanceString>();
    auto lisProp = vmi->propertyValue("lis")->as<rive::ViewModelInstanceList>();

    // Number value changes once triggering a state transition
    silver.addFrame();
    numProp->propertyValue(20);
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Setting number property to same value doesn't trigger state transition
    silver.addFrame();
    numProp->propertyValue(20);
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Updating number property twice triggers single state transition
    silver.addFrame();
    numProp->propertyValue(10);
    numProp->propertyValue(20);
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Updating two properties triggers transition in different layers
    silver.addFrame();
    numProp->propertyValue(10);
    triggerProp->trigger();
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Updating color property twice triggers state transition
    silver.addFrame();
    colorProp->propertyValue(rive::colorARGB(100, 0, 10, 15));
    colorProp->propertyValue(rive::colorARGB(101, 0, 10, 15));
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Updating color property once more triggers state transition
    silver.addFrame();
    colorProp->propertyValue(rive::colorARGB(102, 0, 10, 15));
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Updating boolean property twice triggers state transition
    silver.addFrame();
    bolProp->propertyValue(true);
    bolProp->propertyValue(false);
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Updating boolean property once more triggers state transition
    silver.addFrame();
    bolProp->propertyValue(true);
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Updating string property twice triggers state transition
    silver.addFrame();
    stringProp->propertyValue("a");
    stringProp->propertyValue("b");
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Updating string property once more triggers state transition
    silver.addFrame();
    stringProp->propertyValue("c");
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    // Updating list property by adding two items to the list triggers a single
    // state transition
    {
        silver.addFrame();
        auto itemList = rive::make_rcp<rive::ViewModelInstanceListItem>();
        lisProp->addItem(itemList);
        auto itemList2 = rive::make_rcp<rive::ViewModelInstanceListItem>();
        lisProp->addItem(itemList2);
        stateMachine->advanceAndApply(0.0f);
        artboard->draw(renderer.get());
    }

    // Updating list property by adding one item to the list triggers a single
    // state transition
    {
        silver.addFrame();
        auto itemList = rive::make_rcp<rive::ViewModelInstanceListItem>();
        lisProp->addItem(itemList);
        stateMachine->advanceAndApply(0.0f);
        artboard->draw(renderer.get());
    }

    // Updating list property by adding one item at a specific position triggers
    // a state transition
    {
        silver.addFrame();
        auto itemList = rive::make_rcp<rive::ViewModelInstanceListItem>();
        lisProp->addItemAt(itemList, 0);
        stateMachine->advanceAndApply(0.0f);
        artboard->draw(renderer.get());
    }

    // Updating list property by adding one item at a at an invalid position
    // does not trigger a state transition
    {
        silver.addFrame();
        auto itemList = rive::make_rcp<rive::ViewModelInstanceListItem>();
        lisProp->addItemAt(itemList, 10);
        stateMachine->advanceAndApply(0.0f);
        artboard->draw(renderer.get());
    }

    // Updating list property swapping items triggers a state transition
    {
        silver.addFrame();
        lisProp->swap(0, 1);
        stateMachine->advanceAndApply(0.0f);
        artboard->draw(renderer.get());
    }

    // Removing item from list by index triggers a state transition
    {
        silver.addFrame();
        lisProp->removeItem(0);
        stateMachine->advanceAndApply(0.0f);
        artboard->draw(renderer.get());
    }

    // Removing item from list by index outside of range doesn't trigger a state
    // transition
    {
        silver.addFrame();
        lisProp->removeItem(10);
        stateMachine->advanceAndApply(0.0f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("transition_self_comparator_test"));
}

TEST_CASE("Computed root transform nested artboard", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/computed_root_transform.riv", &silver);

    auto artboard = file->artboardNamed("nested-artboard-main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("computed_root_transform-nested_artboard"));
}

TEST_CASE("Computed root transform list", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/computed_root_transform.riv", &silver);

    auto artboard = file->artboardNamed("list-main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("computed_root_transform-list"));
}

TEST_CASE("Trigger based listeners", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/trigger_based_listeners.riv", &silver);

    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    stateMachine->pointerDown(rive::Vec2D(25.0f, 25.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    stateMachine->pointerUp(rive::Vec2D(25.0f, 25.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);

    silver.addFrame();
    artboard->draw(renderer.get());

    CHECK(silver.matches("trigger_based_listeners"));
}

TEST_CASE("Custom Property Trigger Binding", "[data binding]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/custom_property_trigger.riv", &silver);

    auto artboard = file->artboard("Main")->instance();
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(viewModelInstance);
    REQUIRE(machine != nullptr);
    // Advance state machine
    machine->advanceAndApply(0.0f);

    auto circle = artboard->find<rive::Shape>("MainCircle");
    REQUIRE(circle != nullptr);
    REQUIRE(circle->scaleX() == 1.0f);
    REQUIRE(circle->scaleY() == 1.0f);

    auto trig = artboard->find<rive::CustomPropertyTrigger>("Trig");
    REQUIRE(trig != nullptr);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.16f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        machine->advanceAndApply(0.16f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("custom_property_trigger_bind"));
}

TEST_CASE("Data binding solos", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/data_bind_solo.riv", &silver);

    auto artboard = file->artboardNamed("values-to-solos");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("data_bind_solo-values-to-solos"));
}

TEST_CASE("Data binding solos - target to source", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/data_bind_solo.riv", &silver);

    auto artboard = file->artboardNamed("solos-to-values");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("data_bind_solo-solos-to-values"));
}

TEST_CASE("State machine fire triggers", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/state_transition_fire_trigger.riv", &silver);

    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);

    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("state_transition_fire_trigger"));
}

TEST_CASE("Custom enum properties", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/custom_property_enum.riv", &silver);

    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(3.0f / 0.048f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.048f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("custom_property_enum"));
}

TEST_CASE("View model runtime properties", "[data binding]")
{

    auto file = ReadRiveFile("assets/viewmodel_runtime_file.riv");

    auto vm = file->viewModelByName("vm");
    REQUIRE(vm != nullptr);
    auto instance = vm->createDefaultInstance();
    REQUIRE(instance != nullptr);
    // Number
    auto num = instance->propertyNumber("num");
    REQUIRE(num != nullptr);
    REQUIRE(num->dataType() == rive::DataType::number);
    // string
    auto str = instance->propertyString("str");
    REQUIRE(str != nullptr);
    REQUIRE(str->dataType() == rive::DataType::string);
    // Requesting a property with the same name from cache doesn't return the
    // wrong object
    auto strWrong = instance->propertyNumber("str");
    REQUIRE(strWrong == nullptr);
    // Boolean
    auto boo = instance->propertyBoolean("boo");
    REQUIRE(boo != nullptr);
    REQUIRE(boo->dataType() == rive::DataType::boolean);
    // Color
    auto col = instance->propertyColor("col");
    REQUIRE(col != nullptr);
    REQUIRE(col->dataType() == rive::DataType::color);
    // Trigger
    auto tri = instance->propertyTrigger("tri");
    REQUIRE(tri != nullptr);
    REQUIRE(tri->dataType() == rive::DataType::trigger);
    // Enum
    auto enu = instance->propertyEnum("enu");
    REQUIRE(enu != nullptr);
    REQUIRE(enu->dataType() == rive::DataType::enumType);
    // Image
    auto ima = instance->propertyImage("ima");
    REQUIRE(ima != nullptr);
    REQUIRE(ima->dataType() == rive::DataType::assetImage);
    // Artboard
    auto art = instance->propertyArtboard("art");
    REQUIRE(art != nullptr);
    REQUIRE(art->dataType() == rive::DataType::artboard);
    // List
    auto lis = instance->propertyList("lis");
    REQUIRE(lis != nullptr);
    REQUIRE(lis->dataType() == rive::DataType::list);
    // number in nested view model: chi > num
    auto numChi = instance->propertyNumber("chi/chi-num");
    REQUIRE(numChi != nullptr);
    REQUIRE(numChi->dataType() == rive::DataType::number);
}

TEST_CASE("Trigger fires single change on listener", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/trigger_fires_single_change.riv", &silver);

    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->pointerDown(rive::Vec2D(225.0f, 275.0f));
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    stateMachine->pointerUp(rive::Vec2D(225.0f, 275.0f));
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    silver.addFrame();
    artboard->draw(renderer.get());

    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->pointerDown(rive::Vec2D(225.0f, 275.0f));
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    stateMachine->pointerUp(rive::Vec2D(225.0f, 275.0f));
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(1.0f);
    silver.addFrame();
    artboard->draw(renderer.get());

    CHECK(silver.matches("trigger_fires_single_change"));
}

TEST_CASE("Convert to number", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/data_converter_to_number.riv", &silver);

    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = (int)(1.2f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("data_converter_to_number"));
}

TEST_CASE("List to path", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/list_to_path.riv", &silver);

    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    auto listProp =
        vmi->propertyValue("lis")->as<rive::ViewModelInstanceList>();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    // Create a square with 4 XY vertices

    auto vertex1 = file->createViewModelInstance("vertex-x-y");
    auto vertexInstanceListItem1 =
        rive::make_rcp<rive::ViewModelInstanceListItem>();
    vertexInstanceListItem1->viewModelInstance(vertex1);
    listProp->addItem(vertexInstanceListItem1);

    auto vertex2 = file->createViewModelInstance("vertex-x-y");
    vertex2->propertyValue("x")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(100);
    auto vertexInstanceListItem2 =
        rive::make_rcp<rive::ViewModelInstanceListItem>();
    vertexInstanceListItem2->viewModelInstance(vertex2);
    listProp->addItem(vertexInstanceListItem2);

    auto vertex3 = file->createViewModelInstance("vertex-x-y");
    vertex3->propertyValue("x")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(100);
    vertex3->propertyValue("y")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(100);
    auto vertexInstanceListItem3 =
        rive::make_rcp<rive::ViewModelInstanceListItem>();
    vertexInstanceListItem3->viewModelInstance(vertex3);
    listProp->addItem(vertexInstanceListItem3);

    auto vertex4 = file->createViewModelInstance("vertex-x-y");
    vertex4->propertyValue("y")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(100);
    auto vertexInstanceListItem4 =
        rive::make_rcp<rive::ViewModelInstanceListItem>();
    vertexInstanceListItem4->viewModelInstance(vertex4);
    listProp->addItem(vertexInstanceListItem4);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Insert a mirrored vertex at index 2
    auto vertexRD1 = file->createViewModelInstance("vertex-rotation-distance");
    vertexRD1->propertyValue("x")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(200);
    vertexRD1->propertyValue("rotation")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(1.5f);
    vertexRD1->propertyValue("distance")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(20);
    auto vertexInstanceListItemRD1 =
        rive::make_rcp<rive::ViewModelInstanceListItem>();
    vertexInstanceListItemRD1->viewModelInstance(vertexRD1);
    listProp->addItemAt(vertexInstanceListItemRD1, 2);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Insert a detached vertex at index 3
    auto vertexD1 = file->createViewModelInstance("vertex-detached");
    vertexD1->propertyValue("x")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(200);
    vertexD1->propertyValue("y")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(100);
    vertexD1->propertyValue("inRotation")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(1);
    vertexD1->propertyValue("outRotation")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(2);
    vertexD1->propertyValue("inDistance")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(10);
    vertexD1->propertyValue("outDistance")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(30);
    auto vertexInstanceListItemD1 =
        rive::make_rcp<rive::ViewModelInstanceListItem>();
    vertexInstanceListItemD1->viewModelInstance(vertexD1);
    listProp->addItemAt(vertexInstanceListItemD1, 3);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Insert a cubic in out vertex at index 4
    auto vertexIO1 = file->createViewModelInstance("vertex-in-out");
    vertexIO1->propertyValue("x")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(100);
    vertexIO1->propertyValue("y")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(200);
    vertexIO1->propertyValue("inX")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(40);
    vertexIO1->propertyValue("inY")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(20);
    vertexIO1->propertyValue("outX")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(10);
    vertexIO1->propertyValue("outY")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(30);
    auto vertexInstanceListItemIO1 =
        rive::make_rcp<rive::ViewModelInstanceListItem>();
    vertexInstanceListItemIO1->viewModelInstance(vertexIO1);
    listProp->addItemAt(vertexInstanceListItemIO1, 4);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Insert a non valid vertex at index 4
    auto vertexN1 = file->createViewModelInstance("non-vertex");
    auto vertexInstanceListItemN1 =
        rive::make_rcp<rive::ViewModelInstanceListItem>();
    vertexInstanceListItemN1->viewModelInstance(vertexN1);
    listProp->addItemAt(vertexInstanceListItemN1, 5);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Insert a vertex with some paired values undefined
    auto vertexI1 = file->createViewModelInstance("vertex-incomplete");

    vertexI1->propertyValue("x")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(100);
    vertexI1->propertyValue("y")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(300);
    vertexI1->propertyValue("inDistance")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(60);
    vertexI1->propertyValue("inRotation")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(-1);
    vertexI1->propertyValue("outX")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(30);
    vertexI1->propertyValue("inX")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(-30);
    auto vertexInstanceListItemI1 =
        rive::make_rcp<rive::ViewModelInstanceListItem>();
    vertexInstanceListItemI1->viewModelInstance(vertexI1);
    listProp->addItemAt(vertexInstanceListItemI1, 4);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    silver.addFrame();
    // Update some values to trigger dirt updates
    vertexI1->propertyValue("inX")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(-30);

    vertex1->propertyValue("x")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(50);

    vertexRD1->propertyValue("rotation")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(1.0f);

    vertexD1->propertyValue("inDistance")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(30);

    vertexIO1->propertyValue("outY")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(40);

    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    for (int i = 0; i < 60; i++)
    {
        silver.addFrame();
        vertexI1->propertyValue("inRotation")
            ->as<rive::ViewModelInstanceNumber>()
            ->propertyValue((float)i * 6);
        vertexRD1->propertyValue("rotation")
            ->as<rive::ViewModelInstanceNumber>()
            ->propertyValue((float)i * 6);
        stateMachine->advanceAndApply(0.01f);
        stateMachine->advanceAndApply(0.0f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("list_to_path"));
}

TEST_CASE("Format text with commas", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/format_number_with_commas.riv", &silver);

    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    CHECK(silver.matches("format_number_with_commas"));
}

TEST_CASE("Interpolate color and number", "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/time_based_interpolation.riv", &silver);

    auto artboard = file->artboardNamed("main");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    stateMachine->pointerDown(rive::Vec2D(25.0f, 25.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);
    stateMachine->pointerUp(rive::Vec2D(25.0f, 25.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);

    silver.addFrame();
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.032f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.032f);
        artboard->draw(renderer.get());
    }

    stateMachine->pointerDown(rive::Vec2D(425.0f, 25.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);
    stateMachine->pointerUp(rive::Vec2D(425.0f, 25.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);

    silver.addFrame();
    artboard->draw(renderer.get());

    for (int i = 0; i < 10; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.032f);
        artboard->draw(renderer.get());
    }

    // Interrupt interpolation mid way
    stateMachine->pointerDown(rive::Vec2D(25.0f, 25.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);
    stateMachine->pointerUp(rive::Vec2D(25.0f, 25.0f));
    // Advance and apply twice to take the transition and apply the next state.
    stateMachine->advanceAndApply(0.016f);
    stateMachine->advanceAndApply(0.016f);

    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.032f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("time_based_interpolation"));
}

TEST_CASE("Bidirectional data binding with source to target precedence",
          "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/bidirectional_precedence.riv", &silver);

    auto artboard = file->artboardNamed("source_first");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    auto xProp = vmi->propertyValue("x")->as<rive::ViewModelInstanceNumber>();
    auto yProp = vmi->propertyValue("y")->as<rive::ViewModelInstanceNumber>();

    // On source first these values will overwrite the target values
    // that are [250,250]
    xProp->propertyValue(100);
    yProp->propertyValue(100);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    CHECK(silver.matches("bidirectional_precedence-source_first"));
}

TEST_CASE("Bidirectional data binding with target to source precedence",
          "[data binding]")
{

    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/bidirectional_precedence.riv", &silver);

    auto artboard = file->artboardNamed("target_first");

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    auto xProp = vmi->propertyValue("x")->as<rive::ViewModelInstanceNumber>();
    auto yProp = vmi->propertyValue("y")->as<rive::ViewModelInstanceNumber>();

    // On target first these values will be overwritten by the target values
    // that are [250,250]
    xProp->propertyValue(100);
    yProp->propertyValue(100);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    CHECK(silver.matches("bidirectional_precedence-target_first"));
}