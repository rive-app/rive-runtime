#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/container_component.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/layout_component.hpp"
#include "rive/transform_component.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive_file_reader.hpp"
#include "yoga/YGNode.h"
#include "yoga/Yoga.h"
#include "yoga/event/event.h"
#include <catch.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

// A stack-heavy HUD laid out with the old 1fr stack cell and with
// minmax(0, 1fr); hidden from normal runs.
namespace
{
struct BenchCounts
{
    long layouts = 0;
    long measures = 0;
    long callbacks = 0;
    long probes = 0;
};
BenchCounts gBench;

struct BenchText
{
    int chars;
};

// Text-like leaf; a probe's zero offer reads as unbounded, as Rive's does.
YGSize measureBenchText(YGNodeRef node,
                        float width,
                        YGMeasureMode widthMode,
                        float,
                        YGMeasureMode)
{
    gBench.callbacks++;
    const bool probing = YGConfigIsMeasuringMinContent(YGNodeGetConfig(node));
    if (probing)
    {
        gBench.probes++;
    }
    const float natural =
        static_cast<BenchText*>(YGNodeGetContext(node))->chars * 7.0f;
    const bool unbounded =
        widthMode == YGMeasureModeUndefined ||
        (probing && widthMode == YGMeasureModeAtMost && !(width > 0));
    const float measured = unbounded ? natural
                           : widthMode == YGMeasureModeExactly
                               ? width
                               : std::min(width, natural);
    const float lines = std::ceil(natural / std::max(measured, 1.0f));
    return YGSize{measured, lines * 16.0f};
}

struct BenchHud
{
    YGConfigRef config = nullptr;
    YGNodeRef root = nullptr;
    YGNodeRef timer = nullptr;
    std::vector<std::unique_ptr<BenchText>> texts;
};

YGNodeRef benchText(BenchHud& hud, int chars)
{
    hud.texts.push_back(std::make_unique<BenchText>(BenchText{chars}));
    YGNodeRef node = YGNodeNewWithConfig(hud.config);
    YGNodeSetContext(node, hud.texts.back().get());
    YGNodeSetMeasureFunc(node, measureBenchText);
    return node;
}

YGNodeRef benchStack(BenchHud& hud, bool minmaxCell, YGJustify justify)
{
    YGNodeRef stack = YGNodeNewWithConfig(hud.config);
    YGNodeStyleSetDisplay(stack, YGDisplayGrid);
    YGNodeStyleSetGridTemplateColumnsCount(stack, 1);
    YGNodeStyleSetGridTemplateRowsCount(stack, 1);
    if (minmaxCell)
    {
        YGNodeStyleSetGridTemplateColumnMinMax(stack,
                                               0,
                                               YGGridTrackTypePoints,
                                               0,
                                               YGGridTrackTypeFr,
                                               1);
        YGNodeStyleSetGridTemplateRowMinMax(stack,
                                            0,
                                            YGGridTrackTypePoints,
                                            0,
                                            YGGridTrackTypeFr,
                                            1);
    }
    else
    {
        YGNodeStyleSetGridTemplateColumn(stack, 0, YGGridTrackTypeFr, 1);
        YGNodeStyleSetGridTemplateRow(stack, 0, YGGridTrackTypeFr, 1);
    }
    YGNodeStyleSetJustifyItems(stack, justify);
    YGNodeStyleSetAlignItems(stack, YGAlignCenter);
    return stack;
}

void benchAdd(YGNodeRef parent, YGNodeRef child)
{
    YGNodeInsertChild(parent, child, YGNodeGetChildCount(parent));
}

// Every stack child shares cell 1,1, as syncStackItemCell places it.
void benchStackAdd(YGNodeRef stack, YGNodeRef child)
{
    YGNodeStyleSetGridColumnStart(child, 1);
    YGNodeStyleSetGridRowStart(child, 1);
    benchAdd(stack, child);
}

// Stack roots over a list of rows, each a label plus badge and icon stacks,
// and a timer in its own stack.
BenchHud buildBenchHud(bool minmaxCell, int rows)
{
    BenchHud hud;
    hud.config = YGConfigNew();
    YGConfigSetPointScaleFactor(hud.config, 0);
    hud.root = benchStack(hud, minmaxCell, YGJustifyCenter);
    YGNodeStyleSetWidth(hud.root, 480.0f);
    YGNodeStyleSetHeight(hud.root, 320.0f);

    YGNodeRef panel = benchStack(hud, minmaxCell, YGJustifyStart);
    YGNodeStyleSetJustifySelf(panel, YGJustifyStretch);
    YGNodeStyleSetAlignSelf(panel, YGAlignStretch);
    benchStackAdd(hud.root, panel);

    YGNodeRef list = YGNodeNewWithConfig(hud.config);
    YGNodeStyleSetFlexDirection(list, YGFlexDirectionColumn);
    YGNodeStyleSetPadding(list, YGEdgeAll, 8.0f);
    YGNodeStyleSetGap(list, YGGutterRow, 4.0f);
    YGNodeStyleSetJustifySelf(list, YGJustifyStretch);
    YGNodeStyleSetAlignSelf(list, YGAlignStart);
    benchStackAdd(panel, list);
    for (int i = 0; i < rows; i++)
    {
        YGNodeRef row = YGNodeNewWithConfig(hud.config);
        YGNodeStyleSetFlexDirection(row, YGFlexDirectionRow);
        YGNodeStyleSetAlignItems(row, YGAlignCenter);
        YGNodeStyleSetGap(row, YGGutterColumn, 6.0f);
        benchAdd(row, benchText(hud, 10 + i % 7));
        YGNodeRef spacer = YGNodeNewWithConfig(hud.config);
        YGNodeStyleSetFlexGrow(spacer, 1.0f);
        benchAdd(row, spacer);
        YGNodeRef badge = benchStack(hud, minmaxCell, YGJustifyCenter);
        YGNodeStyleSetPadding(badge, YGEdgeAll, 3.0f);
        benchStackAdd(badge, benchText(hud, 4));
        benchAdd(row, badge);
        YGNodeRef icon = benchStack(hud, minmaxCell, YGJustifyCenter);
        benchStackAdd(icon, benchText(hud, 2));
        benchAdd(row, icon);
        benchAdd(list, row);
    }

    YGNodeRef header = benchStack(hud, minmaxCell, YGJustifyEnd);
    YGNodeStyleSetJustifySelf(header, YGJustifyEnd);
    YGNodeStyleSetAlignSelf(header, YGAlignStart);
    hud.timer = benchText(hud, 5);
    benchStackAdd(header, hud.timer);
    benchStackAdd(panel, header);
    return hud;
}

struct BenchResult
{
    double populateMs;
    double frameUs;
    BenchCounts populate;
    BenchCounts frame;
};

BenchResult runBenchHud(bool minmaxCell, int rows, int frames)
{
    using Clock = std::chrono::steady_clock;
    BenchHud hud = buildBenchHud(minmaxCell, rows);
    BenchResult result{};

    gBench = {};
    auto start = Clock::now();
    YGNodeCalculateLayout(hud.root, 480.0f, 320.0f, YGDirectionLTR);
    result.populateMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    result.populate = gBench;

    gBench = {};
    auto* timer = static_cast<BenchText*>(YGNodeGetContext(hud.timer));
    start = Clock::now();
    for (int frame = 0; frame < frames; frame++)
    {
        timer->chars = 5 + frame % 2;
        YGNodeMarkDirty(hud.timer);
        YGNodeCalculateLayout(hud.root, 480.0f, 320.0f, YGDirectionLTR);
    }
    result.frameUs =
        std::chrono::duration<double, std::micro>(Clock::now() - start)
            .count() /
        frames;
    result.frame = gBench;

    YGNodeFreeRecursive(hud.root);
    YGConfigFree(hud.config);
    return result;
}

} // namespace

TEST_CASE("Bench stack HUD", "[.bench]")
{
    const int frames =
        getenv("BENCH_FRAMES") ? atoi(getenv("BENCH_FRAMES")) : 2000;
    REQUIRE(frames > 0);
    facebook::yoga::Event::subscribe([](const YGNode&,
                                        facebook::yoga::Event::Type type,
                                        facebook::yoga::Event::Data data) {
        if (type == facebook::yoga::Event::LayoutPassEnd)
        {
            auto* layout =
                data.get<facebook::yoga::Event::LayoutPassEnd>().layoutData;
            gBench.layouts += layout->layouts;
            gBench.measures += layout->measures;
        }
    });
    for (int rows : {10, 50, 100})
    {
        // Interleaved rounds; the fastest of each is reported.
        BenchResult best[2] = {};
        for (int round = 0; round < 5; round++)
        {
            for (int cell = 0; cell < 2; cell++)
            {
                BenchResult r = runBenchHud(cell == 1, rows, frames);
                if (round == 0 || r.frameUs < best[cell].frameUs)
                {
                    best[cell] = r;
                }
            }
        }
        for (int cell = 0; cell < 2; cell++)
        {
            const BenchResult& r = best[cell];
            printf("BENCH rows %3d  %-14s  populate %7.3f ms (%5ld layouts "
                   "%5ld measures %4ld probes)  frame %8.2f us  per frame: "
                   "%6.1f layouts %6.1f measures %6.1f measure calls %5.1f "
                   "probes\n",
                   rows,
                   cell == 1 ? "minmax(0,1fr)" : "1fr (master)",
                   r.populateMs,
                   r.populate.layouts,
                   r.populate.measures,
                   r.populate.probes,
                   r.frameUs,
                   r.frame.layouts / double(frames),
                   r.frame.measures / double(frames),
                   r.frame.callbacks / double(frames),
                   r.frame.probes / double(frames));
        }
    }
    facebook::yoga::Event::reset();
}

namespace
{
// Every value a view model reaches, nested view models included.
void collectAllValues(rive::ViewModelInstance* instance,
                      std::vector<rive::ViewModelInstanceValue*>& out,
                      int depth)
{
    if (instance == nullptr || depth > 8)
    {
        return;
    }
    for (auto& value : instance->propertyValues())
    {
        out.push_back(value.get());
        if (value->is<rive::ViewModelInstanceViewModel>())
        {
            collectAllValues(value->as<rive::ViewModelInstanceViewModel>()
                                 ->referenceViewModelInstance()
                                 .get(),
                             out,
                             depth + 1);
        }
    }
}

std::string benchName(rive::ViewModelInstanceValue* value)
{
    return value->viewModelProperty() != nullptr
               ? value->viewModelProperty()->name()
               : std::string();
}

bool benchListed(const char* names, const std::string& name)
{
    return names != nullptr &&
           ("," + std::string(names) + ",").find("," + name + ",") !=
               std::string::npos;
}

// A clock reading for a ticked number, by its name's unit.
float benchClock(const std::string& name, int tick)
{
    if (name.find("Min") != std::string::npos)
    {
        return (float)(tick / 3600 % 60);
    }
    if (name.find("Sec") != std::string::npos)
    {
        return (float)(tick / 60 % 60);
    }
    return (float)(tick % 60 * 16);
}

struct FileBenchResult
{
    double populateMs;
    BenchCounts populate;
    double meanUs, p50Us, p95Us;
    BenchCounts frame, warmup, late;
    double warmupMaxMs, lateMs;
    size_t ticking, perSecond;
};

void benchDumpComponent(FILE* out,
                        rive::Component* component,
                        const std::string& path);

// Every transform's world origin and every layout's box, keyed by tree path.
void benchDumpArtboard(FILE* out,
                       rive::Artboard* artboard,
                       const std::string& path)
{
    benchDumpComponent(out, artboard, path + "/" + artboard->name());
}

void benchDumpComponent(FILE* out,
                        rive::Component* component,
                        const std::string& path)
{
    if (component->is<rive::TransformComponent>())
    {
        const auto& world =
            component->as<rive::TransformComponent>()->worldTransform();
        fprintf(out,
                "%s t%u collapsed %d world %.2f %.2f",
                path.c_str(),
                (unsigned)component->coreType(),
                (int)component->isCollapsed(),
                world[4],
                world[5]);
        if (component->is<rive::LayoutComponent>())
        {
            auto* layout = component->as<rive::LayoutComponent>();
            fprintf(out,
                    " box %.2f %.2f %.2f %.2f",
                    layout->layoutX(),
                    layout->layoutY(),
                    layout->layoutWidth(),
                    layout->layoutHeight());
        }
        fprintf(out, "\n");
    }
    if (component->is<rive::ArtboardComponentList>())
    {
        auto* list = component->as<rive::ArtboardComponentList>();
        for (size_t i = 0; i < list->artboardCount(); i++)
        {
            if (auto* row = list->artboardInstance((int)i))
            {
                benchDumpArtboard(out,
                                  row,
                                  path + "[" + std::to_string(i) + "]");
            }
        }
    }
    else if (component->is<rive::NestedArtboard>())
    {
        if (auto* inner =
                component->as<rive::NestedArtboard>()->artboardInstance())
        {
            benchDumpArtboard(out, inner, path + ">");
        }
    }
    if (component->is<rive::ContainerComponent>())
    {
        int index = 0;
        for (auto* child :
             component->as<rive::ContainerComponent>()->children())
        {
            benchDumpComponent(out,
                               child,
                               path + "/" + std::to_string(index++));
        }
    }
}

// Diagnostic: every list and nested artboard reachable from an artboard.
void benchWalk(rive::Artboard* artboard, int depth)
{
    for (auto* object : artboard->objects())
    {
        if (object == nullptr)
        {
            continue;
        }
        if (object->is<rive::ArtboardComponentList>())
        {
            auto* list = object->as<rive::ArtboardComponentList>();
            printf(
                "BENCH walk %*s list \"%s\" in \"%s\" rows %zu collapsed %d\n",
                depth * 2,
                "",
                list->name().c_str(),
                artboard->name().c_str(),
                list->artboardCount(),
                (int)list->isCollapsed());
            for (auto* up = list->parent(); up != nullptr; up = up->parent())
            {
                printf("BENCH walk %*s   up type %u collapsed %d\n",
                       depth * 2,
                       "",
                       (unsigned)up->coreType(),
                       (int)up->isCollapsed());
            }
            for (size_t i = 0; i < list->artboardCount() && i < 1; i++)
            {
                if (auto* row = list->artboardInstance((int)i))
                {
                    benchWalk(row, depth + 1);
                }
            }
        }
        else if (object->is<rive::NestedArtboard>())
        {
            auto* nested = object->as<rive::NestedArtboard>();
            printf("BENCH walk %*s nested \"%s\" collapsed %d\n",
                   depth * 2,
                   "",
                   nested->name().c_str(),
                   (int)nested->isCollapsed());
            if (auto* inner = nested->artboardInstance())
            {
                benchWalk(inner, depth + 1);
            }
        }
    }
}

FileBenchResult runFileBench(const char* path, int listCount, int frames)
{
    using Clock = std::chrono::steady_clock;
    const float dt = 1.0f / 60.0f;
    auto file = ReadRiveFile(path);
    auto artboard = getenv("BENCH_ARTBOARD")
                        ? file->artboardNamed(getenv("BENCH_ARTBOARD"))
                        : file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto machine = artboard->defaultStateMachine();

    // BENCH_VM + BENCH_VMI pick a named instance, else the default one.
    auto viewModel = getenv("BENCH_VM") && getenv("BENCH_VMI")
                         ? file->createViewModelInstance(getenv("BENCH_VM"),
                                                         getenv("BENCH_VMI"))
                         : file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModel != nullptr);
    if (machine != nullptr)
    {
        machine->bindViewModelInstance(viewModel);
    }
    else
    {
        artboard->bindViewModelInstance(viewModel);
    }

    std::vector<rive::ViewModelInstanceValue*> all;
    collectAllValues(viewModel.get(), all, 0);
    const char* tickNames = getenv("BENCH_TICK");
    const char* everySecond = getenv("BENCH_EVERY_SECOND");
    const char* listName = getenv("BENCH_LIST");
    std::vector<rive::ViewModelInstanceValue*> ticking, perSecond;
    rive::ViewModelInstanceList* list = nullptr;
    for (auto* value : all)
    {
        const std::string name = benchName(value);
        if (benchListed(tickNames, name))
        {
            ticking.push_back(value);
        }
        else if (everySecond != nullptr &&
                 value->is<rive::ViewModelInstanceNumber>() &&
                 name.rfind(everySecond, 0) == 0)
        {
            perSecond.push_back(value);
        }
        if (listName != nullptr && name == listName &&
            value->is<rive::ViewModelInstanceList>())
        {
            list = value->as<rive::ViewModelInstanceList>();
        }
    }

    // BENCH_DUMP prints booleans and enums; BENCH_SET="a=1,b=0" sets them.
    const char* sets = getenv("BENCH_SET");
    for (auto* value : all)
    {
        const std::string name = benchName(value);
        const bool isBool = value->is<rive::ViewModelInstanceBoolean>();
        const bool isEnum = value->is<rive::ViewModelInstanceEnum>();
        if (!isBool && !isEnum)
        {
            continue;
        }
        if (sets != nullptr)
        {
            const std::string key = name + "=";
            const std::string joined = std::string(",") + sets;
            auto at = joined.find("," + key);
            if (at != std::string::npos)
            {
                int set = atoi(joined.c_str() + at + 1 + key.size());
                if (isBool)
                {
                    value->as<rive::ViewModelInstanceBoolean>()->propertyValue(
                        set != 0);
                }
                else
                {
                    value->as<rive::ViewModelInstanceEnum>()->propertyValue(
                        (uint32_t)set);
                }
            }
        }
        if (getenv("BENCH_DUMP"))
        {
            printf("BENCH value %s %s = %d\n",
                   isBool ? "bool" : "enum",
                   name.c_str(),
                   isBool ? (int)value->as<rive::ViewModelInstanceBoolean>()
                                ->propertyValue()
                          : (int)value->as<rive::ViewModelInstanceEnum>()
                                ->propertyValue());
        }
    }

    // BENCH_LIST refilled with listCount BENCH_LIST_VM items; -1 keeps it.
    auto fillList = [&]() {
        if (listCount >= 0 && list != nullptr && getenv("BENCH_LIST_VM"))
        {
            while (!list->listItems().empty())
            {
                list->removeItem(0);
            }
            auto* itemModel = file->viewModel(getenv("BENCH_LIST_VM"));
            REQUIRE(itemModel != nullptr);
            for (int i = 0; i < listCount; i++)
            {
                auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
                item->viewModelInstance(
                    file->createViewModelInstance(itemModel));
                list->addItem(item);
            }
            printf("BENCH list items %zu\n", list->listItems().size());
        }
    };
    const bool late = getenv("BENCH_LIST_LATE") != nullptr;
    if (!late)
    {
        fillList();
    }

    int tick = 0;
    uint32_t seed = 1;
    auto advance = [&]() {
        tick++;
        for (auto* value : ticking)
        {
            if (value->is<rive::ViewModelInstanceNumber>())
            {
                value->as<rive::ViewModelInstanceNumber>()->propertyValue(
                    benchClock(benchName(value), tick));
            }
        }
        if (tick % 60 == 0)
        {
            for (auto* value : perSecond)
            {
                seed = seed * 1664525u + 1013904223u;
                value->as<rive::ViewModelInstanceNumber>()->propertyValue(
                    (seed >> 8) / float(1 << 24));
            }
        }
        if (machine != nullptr)
        {
            machine->advanceAndApply(dt);
        }
        else
        {
            artboard->advance(dt);
        }
    };

    FileBenchResult result{};
    result.ticking = ticking.size();
    result.perSecond = perSecond.size();
    gBench = {};
    auto start = Clock::now();
    advance();
    result.populateMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    result.populate = gBench;
    gBench = {};
    for (int i = 0; i < 60; i++)
    {
        auto warmStart = Clock::now();
        advance();
        result.warmupMaxMs = std::max(
            result.warmupMaxMs,
            std::chrono::duration<double, std::milli>(Clock::now() - warmStart)
                .count());
    }
    result.warmup = gBench;
    if (late)
    {
        fillList();
        gBench = {};
        auto lateStart = Clock::now();
        advance();
        result.lateMs =
            std::chrono::duration<double, std::milli>(Clock::now() - lateStart)
                .count();
        result.late = gBench;
        for (int i = 0; i < 60; i++)
        {
            advance();
        }
    }
    if (const char* boundsPath = getenv("BENCH_BOUNDS"))
    {
        auto name =
            std::string(boundsPath) + "_" + std::to_string(listCount) + ".txt";
        if (FILE* out = fopen(name.c_str(), "w"))
        {
            benchDumpArtboard(out, artboard.get(), "");
            fclose(out);
        }
    }
    if (getenv("BENCH_WALK"))
    {
        benchWalk(artboard.get(), 0);
    }

    gBench = {};
    std::vector<double> times;
    times.reserve(frames);
    for (int i = 0; i < frames; i++)
    {
        auto frameStart = Clock::now();
        advance();
        times.push_back(
            std::chrono::duration<double, std::micro>(Clock::now() - frameStart)
                .count());
    }
    result.frame = gBench;
    double total = 0;
    for (double t : times)
    {
        total += t;
    }
    std::sort(times.begin(), times.end());
    result.meanUs = total / frames;
    result.p50Us = times[frames / 2];
    result.p95Us = times[frames * 95 / 100];
    return result;
}
} // namespace

// A real file's state machine (BENCH_RIV), optionally swept over the length
// of one view model list (BENCH_LIST_COUNTS); see runFileBench for the rest.
TEST_CASE("Bench layout file", "[.bench]")
{
    const char* path = getenv("BENCH_RIV");
    if (path == nullptr)
    {
        printf("BENCH set BENCH_RIV to a .riv\n");
        return;
    }
    const int frames =
        getenv("BENCH_FRAMES") ? atoi(getenv("BENCH_FRAMES")) : 600;
    REQUIRE(frames > 0);
    // Times what a shipping runtime does, not the test-only row checks.
    struct QuietRowsGuard
    {
        bool saved = rive::ArtboardComponentList::sm_verifyQuietRows;
        ~QuietRowsGuard()
        {
            rive::ArtboardComponentList::sm_verifyQuietRows = saved;
        }
    } quietRows;
    rive::ArtboardComponentList::sm_verifyQuietRows = false;
    facebook::yoga::Event::subscribe([](const YGNode& node,
                                        facebook::yoga::Event::Type type,
                                        facebook::yoga::Event::Data data) {
        if (type == facebook::yoga::Event::LayoutPassEnd)
        {
            auto* layout =
                data.get<facebook::yoga::Event::LayoutPassEnd>().layoutData;
            gBench.layouts += layout->layouts;
            gBench.measures += layout->measures;
        }
        else if (type == facebook::yoga::Event::MeasureCallbackStart)
        {
            gBench.callbacks++;
            if (YGConfigIsMeasuringMinContent(
                    YGNodeGetConfig(const_cast<YGNode*>(&node))))
            {
                gBench.probes++;
            }
        }
    });
    std::vector<int> counts;
    if (const char* list = getenv("BENCH_LIST_COUNTS"))
    {
        for (const char* p = list; *p != '\0';)
        {
            counts.push_back(atoi(p));
            while (*p != '\0' && *p != ',')
            {
                p++;
            }
            if (*p == ',')
            {
                p++;
            }
        }
    }
    else
    {
        counts.push_back(-1);
    }
    for (int count : counts)
    {
        FileBenchResult r = runFileBench(path, count, frames);
        printf("BENCH list %3d  populate %7.3f ms (%5ld layouts %5ld measures "
               "%5ld probes)  frame mean %8.2f us p50 %8.2f p95 %8.2f  per "
               "frame: %7.1f layouts %7.1f measures %7.1f measure calls %7.1f "
               "probes  (ticking %zu, per second %zu)  warmup max %6.3f ms "
               "(%5ld layouts %5ld probes over 60)  late fill %6.3f ms (%5ld "
               "layouts %5ld measures)\n",
               count,
               r.populateMs,
               r.populate.layouts,
               r.populate.measures,
               r.populate.probes,
               r.meanUs,
               r.p50Us,
               r.p95Us,
               r.frame.layouts / double(frames),
               r.frame.measures / double(frames),
               r.frame.callbacks / double(frames),
               r.frame.probes / double(frames),
               r.ticking,
               r.perSecond,
               r.warmupMaxMs,
               r.warmup.layouts,
               r.warmup.probes,
               r.lateMs,
               r.late.layouts,
               r.late.measures);
    }
    facebook::yoga::Event::reset();
}
