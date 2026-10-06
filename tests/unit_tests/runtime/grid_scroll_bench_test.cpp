#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Scroll benchmark for a virtualized grid of many items, driven through its
// state machine; hidden from normal runs.
TEST_CASE("Bench virtualized grid scroll", "[.bench]")
{
    using Clock = std::chrono::steady_clock;
    auto ms = [](Clock::time_point from) {
        return std::chrono::duration<double, std::milli>(Clock::now() - from)
            .count();
    };
    // Times what a shipping runtime does, not the test-only row checks.
    rive::ArtboardComponentList::sm_verifyQuietRows = false;
    int maxCount = getenv("BENCH_MAX") ? atoi(getenv("BENCH_MAX")) : 100000;
    std::vector<int> counts = {1000, 10000, 100000};
    if (getenv("BENCH_REPEAT"))
    {
        counts.assign(atoi(getenv("BENCH_REPEAT")), 100000);
    }
    for (int count : counts)
    {
        if (count > maxCount)
        {
            break;
        }
        auto file =
            ReadRiveFile("assets/layout/layout_scroll_grid_virtualized.riv");
        auto artboard = file->artboard("Main")->instance();
        if (getenv("BENCH_NOVIRT"))
        {
            artboard->find<rive::ScrollConstraint>()[0]->virtualize(false);
        }
        auto vmi = file->createDefaultViewModelInstance(artboard.get());
        auto sm = artboard->stateMachineCount() > 0
                      ? artboard->defaultStateMachine()
                      : nullptr;
        if (sm != nullptr)
        {
            sm->bindViewModelInstance(vmi);
        }
        else
        {
            artboard->bindViewModelInstance(vmi);
        }
        rive::ViewModelInstanceList* items = nullptr;
        for (auto& value : vmi->propertyValues())
        {
            if (value->is<rive::ViewModelInstanceList>())
            {
                items = value->as<rive::ViewModelInstanceList>();
            }
        }
        REQUIRE(items != nullptr);
        auto first = items->listItems()[0];
        auto viewModel = first->viewModelInstance()->viewModel();
        auto setupStart = Clock::now();
        while ((int)items->listItems().size() < count)
        {
            auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
            item->viewModelInstance(file->createViewModelInstance(viewModel));
            item->artboard(first->artboard());
            if ((int)items->listItems().size() + 1 < count)
            {
                items->internalAddItem(item);
            }
            else
            {
                items->addItem(item);
            }
        }
        double create = ms(setupStart);
        auto frame = [&](float seconds) {
            if (sm != nullptr)
            {
                sm->advanceAndApply(seconds);
            }
            else
            {
                artboard->advance(seconds);
            }
        };
        auto scroll = artboard->find<rive::ScrollConstraint>()[0];
        double firstFrame = 0.0;
        for (int i = 0; i < 3; i++)
        {
            auto frameStart = Clock::now();
            frame(0.0f);
            if (i == 0)
            {
                firstFrame = ms(frameStart);
            }
        }
        double settle = ms(setupStart);
        printf("LOAD items %6d  create+add %8.2f ms  first frame %8.2f ms  "
               "next two %8.2f ms\n",
               count,
               create,
               firstFrame,
               settle - create - firstFrame);
        if (getenv("BENCH_LOAD_ONLY"))
        {
            continue;
        }
        const int frames = 600;
        double worst = 0.0;
        auto start = Clock::now();
        for (int f = 0; f < frames; f++)
        {
            auto frameStart = Clock::now();
            scroll->scrollOffsetY(-(float)f * 25.0f);
            frame(0.016f);
            worst = std::max(worst, ms(frameStart));
        }
        double scrolling = ms(start) / frames;
        start = Clock::now();
        for (int f = 0; f < frames; f++)
        {
            frame(0.016f);
        }
        double idle = ms(start) / frames;
        auto extra = rive::make_rcp<rive::ViewModelInstanceListItem>();
        extra->viewModelInstance(file->createViewModelInstance(viewModel));
        extra->artboard(first->artboard());
        start = Clock::now();
        items->addItem(extra);
        frame(0.016f);
        double append = ms(start);
        printf("APPEND items %6d  add one item + frame %9.2f ms\n",
               count,
               append);
        printf("BENCH %s items %6d  load %9.2f ms  scroll %8.3f ms/frame "
               "(worst %8.3f)  idle %8.3f ms/frame  content %.0f\n",
               sm != nullptr ? "sm" : "ab",
               count,
               settle,
               scrolling,
               worst,
               idle,
               scroll->contentHeight());
    }
    rive::ArtboardComponentList::sm_verifyQuietRows = true;
}
