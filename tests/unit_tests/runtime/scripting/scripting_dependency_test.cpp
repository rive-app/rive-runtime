#include "catch.hpp"
#include "rive_file_reader.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "utils/serializing_factory.hpp"

using namespace rive;

TEST_CASE("scripted data converter number using multi chain requires",
          "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/script_dependency_test.riv", &silver);
    auto artboard = file->artboardNamed("Artboard");

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

    rive::ViewModelInstanceNumber* num =
        vmi->propertyValue("InputValue1")->as<rive::ViewModelInstanceNumber>();
    REQUIRE(num != nullptr);

    int counter = 0;
    int frames = 30;
    for (int i = 0; i < frames; i++)
    {
        num->propertyValue(counter);
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
        counter += 5;
    }

    CHECK(silver.matches("script_converter_with_dependency"));
}

TEST_CASE("scripted data converter string using multi chain requires",
          "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/script_dependency_test2.riv", &silver);
    auto artboard = file->artboardNamed("Artboard");

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

    rive::ViewModelInstanceString* str =
        vmi->propertyValue("InputString")->as<rive::ViewModelInstanceString>();
    REQUIRE(str != nullptr);

    std::vector<std::string> values = {"Hello world!",
                                       "1,2,3",
                                       "rive scripting",
                                       "testing testing testing",
                                       "Script Data Converter"};
    for (int i = 0; i < values.size(); i++)
    {
        str->propertyValue(values[i]);
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("script_converter_with_dependency_2"));
}

// This test file uses 4 published scripts from a library file with a requires
// chain StringConverter -> StringPrefix -> StringUppercase -> StringReverse and
// uses those to convert a string and display it in a text field.
TEST_CASE(
    "scripted data converter string using multi chain requires from library",
    "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/script_dependency_test_using_library.riv",
                             &silver);
    auto artboard = file->artboardNamed("Artboard");

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

    rive::ViewModelInstanceString* str =
        vmi->propertyValue("InputString")->as<rive::ViewModelInstanceString>();
    REQUIRE(str != nullptr);

    std::vector<std::string> values = {"Hello world!",
                                       "1,2,3",
                                       "rive scripting",
                                       "testing testing testing",
                                       "Script Data Converter"};
    for (int i = 0; i < values.size(); i++)
    {
        str->propertyValue(values[i]);
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("script_converter_with_dependency_with_library"));
}

// This test file uses 4 published scripts from the same library file as above
// EXCEPT that it uses v2 of the Scripts - with a requires chain
// StringConverter -> StringPrefix -> StringUppercase -> StringReverse and uses
// those to convert a string and display it in a text field.
TEST_CASE(
    "scripted data converter string using multi chain requires from library with update",
    "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/script_dependency_test_using_library_v2.riv",
                     &silver);
    auto artboard = file->artboardNamed("Artboard");

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

    rive::ViewModelInstanceString* str =
        vmi->propertyValue("InputString")->as<rive::ViewModelInstanceString>();
    REQUIRE(str != nullptr);

    std::vector<std::string> values = {"Hello world!",
                                       "1,2,3",
                                       "rive scripting",
                                       "testing testing testing",
                                       "Script Data Converter"};
    for (int i = 0; i < values.size(); i++)
    {
        str->propertyValue(values[i]);
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches(
        "script_converter_with_dependency_with_library_with_update"));
}

// This test file uses 4 scripts in the following hierarchy:
// - rive
//   - stringutil
//     - StringReverse
//     - StringPrefix
//     - StringUpperCase
//   - converter
//     - StringConverter
// These scripts must have their full qualified namespace included in the
// require For example: require('rive/stringutil/StringReverse')
TEST_CASE("scripted data converter string with namespaced requires", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/script_namespace_test.riv", &silver);
    auto artboard = file->artboardNamed("Artboard");

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

    rive::ViewModelInstanceString* str =
        vmi->propertyValue("InputString")->as<rive::ViewModelInstanceString>();
    REQUIRE(str != nullptr);

    std::vector<std::string> values = {"Hello world!",
                                       "1,2,3",
                                       "rive scripting",
                                       "testing testing testing",
                                       "Script Data Converter"};
    for (int i = 0; i < values.size(); i++)
    {
        str->propertyValue(values[i]);
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("script_namespace_test"));
}