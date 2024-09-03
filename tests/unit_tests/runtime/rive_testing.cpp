#include "rive_testing.hpp"

bool aboutEqual(const rive::Mat2D& a, const rive::Mat2D& b)
{
    const float epsilon = 0.0001f;
    for (int i = 0; i < 6; i++)
    {
        if (std::fabs(a[i] - b[i]) > epsilon)
        {
            return false;
        }
    }
    return true;
}