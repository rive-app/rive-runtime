#include "rive/constraints/scrolling/virtual_layout.hpp"
#include <catch.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Differential sweep: VirtualLayout::window against a transcription of the
// running-sum scan ScrollVirtualizer::virtualize used before it. Mirrors
// scroll_window_sweep_test.dart in rive_core.

using namespace rive;

namespace
{
struct ScanResult
{
    // Realized item (wrapped) -> position along the scroll axis.
    std::map<int, float> realized;
    std::set<int> visible;
};

int wrapIndex(int index, int n)
{
    int wrapped = index % n;
    return wrapped < 0 ? wrapped + n : wrapped;
}

// The old scan, single provider. One deliberate fix: the end walk read sizes
// from a child cursor that compared an item index against the provider count,
// so a window starting at item 1 summed item 0's size. Walking from the start
// item is what it meant.
ScanResult referenceScan(const std::vector<float>& sizes,
                         float gap,
                         float offset,
                         float viewport,
                         bool infinite,
                         int buffer)
{
    int n = (int)sizes.size();
    int start = 0;
    int end = n - 1;
    float runningSize = 0.0f;
    float runningOffset = 0.0f;
    int runningIndex = 0;

    for (int j = 0; j < n; j++)
    {
        float size = sizes[j];
        if (runningSize + size > offset)
        {
            runningOffset = runningSize - offset;
            start = runningIndex;
            break;
        }
        runningSize += size;
        runningIndex++;
        if (runningSize + gap > offset)
        {
            if (runningIndex == n)
            {
                runningIndex = 0;
            }
            runningSize += gap;
            runningOffset = runningSize - offset;
            start = runningIndex;
            break;
        }
        runningSize += gap;
    }

    int i = start;
    int cursor = start;
    bool wrapped = false;
    int cycleCount = 0;
    bool done = false;
    while (!done && i < n && cycleCount < 2)
    {
        for (int j = cursor; j < n; j++)
        {
            float size = sizes[j];
            if (runningSize + size + gap >= offset + viewport)
            {
                end = infinite ? (wrapped ? i + n : i) : i;
                done = true;
                break;
            }
            runningSize += size + gap;
            if (infinite && i == n - 1)
            {
                wrapped = true;
                i = -1;
                cycleCount++;
            }
            i++;
        }
        cursor = 0;
    }

    int visibleStart = start;
    int visibleEnd = end;
    if (buffer > 0 && n > 0)
    {
        int visibleSpan = end - start + 1;
        int maxExtra = std::max(0, n - visibleSpan);
        int before = std::min(buffer, infinite ? maxExtra : start);
        int after =
            std::min(buffer, infinite ? maxExtra - before : n - 1 - end);
        before = std::max(0, before);
        after = std::max(0, after);
        for (int k = 1; k <= before; k++)
        {
            runningOffset -= sizes[wrapIndex(start - k, n)] + gap;
        }
        start -= before;
        end += after;
        if (infinite)
        {
            start += n;
            end += n;
            visibleStart += n;
            visibleEnd += n;
        }
    }

    ScanResult result;
    for (int k = start; k <= end; k++)
    {
        int index = infinite ? k % n : k;
        result.realized[index] = runningOffset;
        if (k >= visibleStart && k <= visibleEnd)
        {
            result.visible.insert(index);
        }
        runningOffset += sizes[index] + gap;
    }
    return result;
}

ScanResult modelScan(const VirtualLayout& layout,
                     float offset,
                     float viewport,
                     bool infinite,
                     int buffer)
{
    int n = layout.lineCount();
    auto window = layout.window(offset, viewport, infinite, buffer);
    ScanResult result;
    for (int line = window.start; line <= window.end; line++)
    {
        int index = wrapIndex(line, n);
        result.realized[index] = layout.lineStart(line) - offset;
        if (window.isVisible(line))
        {
            result.visible.insert(index);
        }
    }
    return result;
}

std::string describe(const std::set<int>& indices)
{
    std::stringstream stream;
    for (int index : indices)
    {
        stream << index << " ";
    }
    return stream.str();
}

std::string compare(const ScanResult& expected, const ScanResult& actual)
{
    std::set<int> expectedKeys, actualKeys;
    for (auto& entry : expected.realized)
    {
        expectedKeys.insert(entry.first);
    }
    for (auto& entry : actual.realized)
    {
        actualKeys.insert(entry.first);
    }
    if (expectedKeys != actualKeys)
    {
        return "realized " + describe(expectedKeys) + "vs " +
               describe(actualKeys);
    }
    for (auto& entry : expected.realized)
    {
        float position = actual.realized.at(entry.first);
        if (std::abs(position - entry.second) > 1e-3f)
        {
            std::stringstream stream;
            stream << "item " << entry.first << " at " << entry.second << " vs "
                   << position;
            return stream.str();
        }
    }
    if (expected.visible != actual.visible)
    {
        return "visible " + describe(expected.visible) + "vs " +
               describe(actual.visible);
    }
    return "";
}

struct Geometry
{
    std::string name;
    std::vector<float> sizes;
    float gap;
    float viewport;
};

std::vector<Geometry> geometries()
{
    // Whole-number sizes keep every sum exact in float, so boundary offsets
    // land exactly where both scans see them.
    std::mt19937 random(7);
    std::vector<float> mixed;
    for (int i = 0; i < 23; i++)
    {
        mixed.push_back(20.0f + (float)(random() % 90));
    }
    std::vector<float> uniform(20, 100.0f);
    std::vector<Geometry> out;
    for (float gap : {0.0f, 10.0f, -10.0f, 37.5f})
    {
        for (float viewport : {130.0f, 300.0f, 333.0f, 640.0f})
        {
            out.push_back({"uniform", uniform, gap, viewport});
            out.push_back({"mixed", mixed, gap, viewport});
        }
    }
    // Viewport smaller than a gap, and tiny lists.
    out.push_back(
        {"gap wider than viewport", std::vector<float>(12, 40.0f), 80, 50});
    out.push_back({"single item", {120.0f}, 10, 90});
    out.push_back({"two items", {60.0f, 90.0f}, 5, 70});
    return out;
}
} // namespace

TEST_CASE("VirtualLayout window matches the old virtualizer scan",
          "[virtual_layout]")
{
    for (auto& geometry : geometries())
    {
        auto& sizes = geometry.sizes;
        VirtualLayout layout;
        layout.buildLinear((int)sizes.size(),
                           [&](int i) { return sizes[i]; },
                           geometry.gap);
        float largest = *std::max_element(sizes.begin(), sizes.end());
        for (bool infinite : {false, true})
        {
            // A carousel viewport longer than one cycle shows an item twice,
            // which neither implementation can realize.
            if (infinite && geometry.viewport + largest >= layout.cycleExtent())
            {
                continue;
            }
            for (int buffer : {0, 1, 2})
            {
                float from = infinite ? 0.0f : -50.0f;
                float to =
                    infinite
                        ? layout.cycleExtent()
                        : std::max(0.0f, layout.extent() - geometry.viewport);
                for (float offset = from; offset <= to; offset += 0.25f)
                {
                    if (infinite && offset >= layout.cycleExtent())
                    {
                        break;
                    }
                    auto expected = referenceScan(sizes,
                                                  geometry.gap,
                                                  offset,
                                                  geometry.viewport,
                                                  infinite,
                                                  buffer);
                    auto actual = modelScan(layout,
                                            offset,
                                            geometry.viewport,
                                            infinite,
                                            buffer);
                    auto mismatch = compare(expected, actual);
                    if (!mismatch.empty())
                    {
                        INFO(geometry.name
                             << " gap " << geometry.gap << " viewport "
                             << geometry.viewport << " infinite " << infinite
                             << " buffer " << buffer << " offset " << offset);
                        FAIL(mismatch);
                    }
                }
            }
        }
    }
}

TEST_CASE("VirtualLayout maps items to lines", "[virtual_layout]")
{
    VirtualLayout layout;
    std::vector<float> sizes = {50, 60, 70};
    layout.buildLinear(3, [&](int i) { return sizes[i]; }, 10);
    CHECK(layout.lineCount() == 3);
    CHECK(layout.extent() == 200);
    CHECK(layout.cycleExtent() == 210);
    CHECK(layout.lineStart(2) == 130);
    // Unwrapped lines step a whole cycle.
    CHECK(layout.lineStart(3) == 210);
    CHECK(layout.lineStart(-1) == 130 - 210);
    CHECK(layout.lineOfItem(1) == 1);
    CHECK(layout.lineFirstItem(4) == 1);
    CHECK(layout.lineLastItem(-1) == 2);
}

TEST_CASE("VirtualLayout windows lines that start before earlier ones",
          "[virtual_layout]")
{
    // A gap more negative than a line is long starts the next line before
    // the last: lines start at 0, 50 and 10, so line 2 sits inside 0..40
    // even though line 1 doesn't.
    VirtualLayout layout;
    std::vector<float> sizes = {100, 10, 10};
    layout.buildLinear(3, [&](int i) { return sizes[i]; }, -50);
    auto window = layout.window(0, 40, false, 0);
    CHECK(window.visibleStart == 0);
    CHECK(window.visibleEnd == 2);
}
