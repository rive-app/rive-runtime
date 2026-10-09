#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/file.hpp"
#include "rive/generated/animation/linear_animation_base.hpp"
#include "rive/generated/animation/state_machine_base.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/runtime_header.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <vector>

// A list row whose artboard has animations but no state machine gets a null
// StateMachineInstance (Artboard::initialize only generates one for an
// artboard with neither). Rows handle that, but
// ArtboardComponentList::internalDataContext walked m_stateMachinesMap without
// a null check: rebinding the list's data context read the null instance's
// data context (Sentry RIVE_NATIVE-142, EXC_BAD_ACCESS at 0x80, the offset of
// StateMachineInstance's m_dataContext).

static std::vector<uint8_t> readAsset(const char* path)
{
    FILE* fp = fopen(path, "rb");
    REQUIRE(fp != nullptr);
    fseek(fp, 0, SEEK_END);
    std::vector<uint8_t> bytes(ftell(fp));
    fseek(fp, 0, SEEK_SET);
    REQUIRE(fread(bytes.data(), 1, bytes.size(), fp) == bytes.size());
    fclose(fp);
    return bytes;
}

// Skips one object, the same way File::readRuntimeObject reads it, and
// returns its type key.
static uint16_t skipObject(rive::BinaryReader& reader,
                           const rive::RuntimeHeader& header)
{
    auto typeKey = reader.readVarUintAs<uint16_t>();
    auto object = rive::CoreRegistry::makeCoreInstance(typeKey);
    while (true)
    {
        auto propertyKey = reader.readVarUintAs<uint16_t>();
        if (propertyKey == 0 || reader.hasError())
        {
            break;
        }
        if (object != nullptr && object->deserialize(propertyKey, reader))
        {
            continue;
        }
        int id = rive::CoreRegistry::propertyFieldId(propertyKey);
        if (id == -1)
        {
            id = header.propertyFieldId(propertyKey);
        }
        switch (id)
        {
            case rive::CoreUintType::id:
                reader.readVarUint64();
                break;
            case rive::CoreBoolType::id:
                rive::CoreBoolType::deserialize(reader);
                break;
            case rive::CoreStringType::id:
                rive::CoreStringType::deserialize(reader);
                break;
            case rive::CoreDoubleType::id:
                rive::CoreDoubleType::deserialize(reader);
                break;
            case rive::CoreColorType::id:
                rive::CoreColorType::deserialize(reader);
                break;
            default:
                FAIL("unknown property " << propertyKey);
        }
    }
    delete object;
    return typeKey;
}

// Rewrites the file so the artboard at artboardIndex has no state machines:
// everything from its first StateMachine up to the next Artboard is replaced
// by one empty LinearAnimation, which keeps the runtime from generating a
// state machine for it.
static std::vector<uint8_t> withoutStateMachines(
    const std::vector<uint8_t>& bytes,
    size_t artboardIndex)
{
    rive::BinaryReader reader(bytes);
    rive::RuntimeHeader header;
    REQUIRE(rive::RuntimeHeader::read(reader, header));
    const uint8_t* base = bytes.data();
    std::vector<uint8_t> out(base, reader.position());

    int artboard = -1;
    bool dropping = false;
    bool replaced = false;
    while (!reader.reachedEnd())
    {
        const uint8_t* start = reader.position();
        auto typeKey = skipObject(reader, header);
        REQUIRE(!reader.hasError());
        if (typeKey == rive::ArtboardBase::typeKey)
        {
            artboard++;
            dropping = false;
        }
        else if (artboard == (int)artboardIndex &&
                 typeKey == rive::StateMachineBase::typeKey && !replaced)
        {
            // VectorBinaryWriter writes from the start of its buffer.
            std::vector<uint8_t> animation;
            rive::VectorBinaryWriter writer(&animation);
            writer.writeVarUint((uint64_t)rive::LinearAnimationBase::typeKey);
            writer.writeVarUint((uint64_t)0);
            out.insert(out.end(), animation.begin(), animation.end());
            dropping = true;
            replaced = true;
        }
        if (!dropping)
        {
            out.insert(out.end(), start, reader.position());
        }
    }
    REQUIRE(replaced);
    return out;
}

static rive::rcp<rive::File> importWithoutItemStateMachines(const char* path)
{
    // Both fixtures keep their list item artboard at index 1.
    auto bytes = withoutStateMachines(readAsset(path), 1);
    rive::ImportResult result;
    auto file = rive::File::import(bytes, &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file != nullptr);
    REQUIRE(file->artboard(1)->stateMachineCount() == 0);
    REQUIRE(file->artboard(1)->animationCount() > 0);
    return file;
}

TEST_CASE("rebinding a list whose rows have no state machine",
          "[component_list]")
{
    auto file = importWithoutItemStateMachines("assets/component_list_1.riv");
    auto artboard = file->artboard("Main")->instance();
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);

    auto list = artboard->find<rive::ArtboardComponentList>("List");
    REQUIRE(list != nullptr);
    REQUIRE(list->artboardCount() > 0);
    for (int i = 0; i < list->artboardCount(); i++)
    {
        REQUIRE(list->artboardInstance(i) != nullptr);
        REQUIRE(list->stateMachineInstance(i) == nullptr);
    }

    // What the editor does when another view model instance is picked:
    // rebind with rows already built.
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);
    CHECK(list->artboardInstance(0)->dataContext() != nullptr);
}

// Scrolling a virtualized list pools the rows it leaves and reuses them for
// the rows it reaches. A row with no state machine used to pool a null one,
// and reusing it called resetState() on null.
TEST_CASE("scrolling a virtualized list whose rows have no state machine",
          "[component_list]")
{
    auto file =
        importWithoutItemStateMachines("assets/component_list_virtualized.riv");
    auto artboard = file->artboard("Main")->instance();
    auto viewModelInstance =
        file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);

    auto scroll = artboard->find<rive::ScrollConstraint>()[0];
    REQUIRE(scroll != nullptr);
    for (int index : {5, 10, 0, 15, 2})
    {
        scroll->setScrollIndex(index);
        artboard->advance(0.0f);
    }
    auto list = artboard->find<rive::ArtboardComponentList>("List");
    REQUIRE(list != nullptr);
    CHECK(list->artboardCount() > 0);
}
