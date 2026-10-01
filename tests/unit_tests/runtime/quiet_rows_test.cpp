#include <rive/file.hpp>
#include <rive/artboard.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <utility>
#include <vector>

// An ArtboardComponentList skips the per-frame work of rows that would do
// nothing ("quiet" rows) until something wakes them. Under TESTING every
// skipped row pass is still run, after the pass, and aborts if it did
// anything, so the whole suite checks the skipping; these cases pin down that
// rows do go quiet and that what the host pushes into them wakes them.

using namespace rive;

namespace
{
struct RowLayout
{
    float width;
    float height;
    float minX;
    float minY;
};

std::vector<RowLayout> rowLayouts(ArtboardComponentList* list)
{
    std::vector<RowLayout> rows;
    for (size_t i = 0; i < list->artboardCount(); i++)
    {
        auto row = list->artboardInstance((int)i);
        auto bounds = list->layoutBoundsForNode((int)i);
        rows.push_back({row->layoutWidth(),
                        row->layoutHeight(),
                        bounds.minX,
                        bounds.minY});
    }
    return rows;
}

bool operator==(const RowLayout& a, const RowLayout& b)
{
    return a.width == b.width && a.height == b.height && a.minX == b.minX &&
           a.minY == b.minY;
}

// Plays "Main" in artboard_list_overrides.riv until its rows could go quiet,
// halves the artboard's width, lets the layout settle, and returns where the
// rows were before and after, with quiet rows on or off.
std::pair<std::vector<RowLayout>, std::vector<RowLayout>> resizeHost(
    File* file,
    bool quietRows)
{
    ArtboardComponentList::sm_quietRowsEnabled = quietRows;
    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    auto viewModelId = artboard->viewModelId();
    auto viewModelInstance =
        viewModelId == -1 ? file->createViewModelInstance(artboard.get())
                          : file->createViewModelInstance(viewModelId, 0);
    stateMachine->bindViewModelInstance(viewModelInstance);
    stateMachine->advanceAndApply(0.1f);

    auto lists = artboard->find<ArtboardComponentList>();
    REQUIRE(lists.size() == 1);
    auto list = lists[0];
    REQUIRE(list->artboardCount() > 0);

    // Longer than the one-second empty animation a state without one plays,
    // which keeps its layer going until it ends.
    auto skips = ArtboardComponentList::sm_quietRowSkips;
    for (int i = 0; i < 90; i++)
    {
        stateMachine->advanceAndApply(1.0f / 60.0f);
    }
    CHECK((ArtboardComponentList::sm_quietRowSkips > skips) == quietRows);
    for (size_t i = 0; i < list->artboardCount(); i++)
    {
        CHECK((list->artboardInstance((int)i)->quietHostRow() == (uint32_t)i) ==
              quietRows);
    }

    auto before = rowLayouts(list);
    artboard->width(artboard->width() * 0.5f);
    for (int i = 0; i < 60; i++)
    {
        stateMachine->advanceAndApply(1.0f / 60.0f);
    }

    auto after = rowLayouts(list);
    ArtboardComponentList::sm_quietRowsEnabled = true;
    return {before, after};
}
} // namespace

TEST_CASE("Quiet list rows wake for layout the host pushes into them",
          "[component_list]")
{
    auto file = ReadRiveFile("assets/artboard_list_overrides.riv");
    auto quiet = resizeHost(file.get(), true);
    auto awake = resizeHost(file.get(), false);
    // The resize must move the rows, or matching proves nothing.
    CHECK_FALSE(quiet.first == quiet.second);
    REQUIRE(quiet.second.size() == awake.second.size());
    CHECK(quiet.second == awake.second);
}
