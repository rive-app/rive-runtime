#include "rive/text.hpp"

using namespace rive;

std::vector<Font::Axis> Font::getAxes() const
{
    std::vector<rive::Font::Axis> axes;
    const uint16_t count = getAxisCount();
    if (count > 0)
    {
        axes.resize(count);

        for (uint16_t i = 0; i < count; ++i)
        {
            axes.push_back(getAxis(i));
        }
    }
    return axes;
}
