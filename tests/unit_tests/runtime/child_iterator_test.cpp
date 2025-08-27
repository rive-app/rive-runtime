#include "rive/file.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

TEST_CASE("child typed iterators work", "[iterators]")
{
    auto file = ReadRiveFile("assets/juice.riv");

    auto artboard = file->artboardDefault();

    // Expect only one node.
    size_t count = 0;
    for (auto child : artboard->children<Node>())
    {
        CHECK(child->name() == "root");
        count++;
    }
    CHECK(count == 1);

    size_t shapePaintCount = 0;
    for (auto child : artboard->children<ShapePaint>())
    {
        shapePaintCount++;
        CHECK(!child->isTranslucent());
    }
    CHECK(shapePaintCount == 1);

    count = 0;
    auto allShapePaints = artboard->objects<ShapePaint>();
    for (auto itr = allShapePaints.begin(); itr != allShapePaints.end(); ++itr)
    {
        count++;
    }
    CHECK(allShapePaints.size() == 37);
    CHECK(allShapePaints.size() == count);
}
