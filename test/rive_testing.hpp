#ifndef _CATCH_RIVE_TESTING_
#define _CATCH_RIVE_TESTING_

#include <catch.hpp>
#include <sstream>
#include <rive/math/mat2d.hpp>

bool aboutEqual(const rive::Mat2D& a, const rive::Mat2D& b);

namespace Catch
{
template <> struct StringMaker<rive::Mat2D>
{
    static std::string convert(rive::Mat2D const& value)
    {
        std::ostringstream os;
        os << value[0] << ", " << value[1] << ", " << value[2] << ", " << value[3] << ", "
           << value[4] << ", " << value[5];
        return os.str();
    }
};
} // namespace Catch
#endif