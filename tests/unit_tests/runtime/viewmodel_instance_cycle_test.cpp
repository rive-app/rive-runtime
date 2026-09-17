#include <rive/file.hpp>
#include <rive/viewmodel/viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_value.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include <rive/viewmodel/viewmodel_property_viewmodel.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>

using namespace rive;

namespace
{
// The first view model reference in the file: which view model and instance
// holds it, the property describing it, and the value carrying it. The tests
// below corrupt these the way a bad export would, which is the only way to
// reach the malformed cases from a well-formed .riv.
struct Reference
{
    size_t viewModelIndex = 0;
    size_t instanceIndex = 0;
    ViewModelPropertyViewModel* property = nullptr;
    ViewModelInstanceViewModel* value = nullptr;

    bool found() const { return property != nullptr; }
};

Reference findReference(File* file)
{
    for (size_t i = 0; i < file->viewModelCount(); i++)
    {
        auto viewModel = file->viewModel(i);
        if (viewModel == nullptr)
        {
            continue;
        }
        for (size_t j = 0; j < viewModel->instanceCount(); j++)
        {
            auto instance = viewModel->instance(j);
            if (instance == nullptr)
            {
                continue;
            }
            for (auto& value : instance->propertyValues())
            {
                if (!value->is<ViewModelInstanceViewModel>())
                {
                    continue;
                }
                auto property =
                    viewModel->property(value->viewModelPropertyId());
                if (property == nullptr ||
                    !property->is<ViewModelPropertyViewModel>())
                {
                    continue;
                }
                return Reference{i,
                                 j,
                                 property->as<ViewModelPropertyViewModel>(),
                                 value->as<ViewModelInstanceViewModel>()};
            }
        }
    }
    return Reference{};
}
} // namespace

TEST_CASE("a view model id past the end of the file is ignored", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/rebind_with_nested_viewmodel.riv");
    auto reference = findReference(file.get());
    REQUIRE(reference.found());

    // An id no view model answers to. Unchecked, this indexed the view model
    // vector out of bounds and called through whatever pointer came back.
    file->viewModel(reference.viewModelIndex)
        ->instance(reference.instanceIndex)
        ->viewModelId(static_cast<uint32_t>(file->viewModelCount()) + 10);

    REQUIRE(file->createViewModelInstance(reference.viewModelIndex,
                                          reference.instanceIndex) != nullptr);
}

TEST_CASE("a nested reference past the end of the file is ignored",
          "[viewmodel]")
{
    auto file = ReadRiveFile("assets/rebind_with_nested_viewmodel.riv");
    auto reference = findReference(file.get());
    REQUIRE(reference.found());

    reference.property->viewModelReferenceId(
        static_cast<uint32_t>(file->viewModelCount()) + 10);

    auto instance = file->createViewModelInstance(reference.viewModelIndex,
                                                  reference.instanceIndex);
    REQUIRE(instance != nullptr);
}

TEST_CASE("a self referential view model instance terminates", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/rebind_with_nested_viewmodel.riv");
    auto reference = findReference(file.get());
    REQUIRE(reference.found());

    // Point the reference back at the instance that holds it. The copy walk
    // marks each source instance before completing its copy, so the cycle
    // terminates rather than cloning forever.
    reference.property->viewModelReferenceId(
        static_cast<uint32_t>(reference.viewModelIndex));
    reference.value->propertyValue(
        static_cast<uint32_t>(reference.instanceIndex));

    auto instance = file->createViewModelInstance(reference.viewModelIndex,
                                                  reference.instanceIndex);
    REQUIRE(instance != nullptr);

    ViewModelInstanceViewModel* copiedValue = nullptr;
    for (auto& value : instance->propertyValues())
    {
        if (value->is<ViewModelInstanceViewModel>())
        {
            copiedValue = value->as<ViewModelInstanceViewModel>();
            break;
        }
    }
    REQUIRE(copiedValue != nullptr);
    // The back-edge is dropped, not wired to the copy: holding it would be a
    // strong reference from the copy to itself, which nothing would ever
    // release.
    REQUIRE(copiedValue->referenceViewModelInstance() == nullptr);
}

TEST_CASE("copying a null view model instance returns null", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/rebind_with_nested_viewmodel.riv");

    // The public entry point is crash-hardening for callers that may not have
    // a source instance, so it has to answer for null rather than clone it.
    REQUIRE(file->copyViewModelInstance(nullptr) == nullptr);
}
