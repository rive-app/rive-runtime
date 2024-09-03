#include "rive/shapes/paint/dash_path.hpp"
#include "utils/no_op_factory.hpp"
#include <catch.hpp>
#include <cstdio>

class _TestDasher : public rive::PathDasher
{

public:
    rive::RenderPath* dash(const rive::RawPath& source,
                           rive::Dash offset,
                           rive::Span<rive::Dash> dashes)
    {
        rive::NoOpFactory noOpFactory;
        return rive::PathDasher::dash(source, &noOpFactory, offset, dashes);
    }
};

TEST_CASE("0 length dashes don't cause a crash", "[dashing]")
{
    rive::RawPath rawPath;
    rawPath.addRect({1, 1, 5, 6});

    _TestDasher dasher;
    std::vector dashes = {rive::Dash(0.0f, false), rive::Dash(0.0f, false)};

    rive::Dash offset(0.0f, false);
    dasher.dash(rawPath, offset, dashes);
}
