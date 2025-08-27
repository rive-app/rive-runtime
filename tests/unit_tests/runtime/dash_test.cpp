#include "rive/shapes/paint/dash_path.hpp"
#include "rive/shapes/paint/dash.hpp"
#include "utils/no_op_factory.hpp"
#include <catch.hpp>
#include <cstdio>

class _TestDasher : public rive::PathDasher
{

public:
    rive::ShapePaintPath* dash(const rive::RawPath* source,
                               rive::Dash* offset,
                               rive::Span<rive::Dash*> dashes)
    {
        return rive::PathDasher::dash(source, offset, dashes);
    }
};

TEST_CASE("0 length dashes don't cause a crash", "[dashing]")
{
    rive::RawPath path;
    path.addRect({1, 1, 5, 6});

    _TestDasher dasher;

    rive::Dash a(0.0f, false);
    rive::Dash b(0.0f, false);
    std::vector<rive::Dash*> dashes = {&a, &b};

    rive::Dash offset(0.0f, false);
    dasher.dash(&path, &offset, dashes);
}
