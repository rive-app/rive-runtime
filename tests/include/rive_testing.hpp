#ifndef _CATCH_RIVE_TESTING_
#define _CATCH_RIVE_TESTING_

#include <catch.hpp>
#include <sstream>
#include "rive/math/mat2d.hpp"
#include "rive/text/cursor.hpp"

bool aboutEqual(const rive::Mat2D& a, const rive::Mat2D& b);

// StringMaker doesn't work for Vec2D.
#define CHECK_VEC2D(a, b)                                                      \
    CHECK(a.x == b.x);                                                         \
    CHECK(a.y == b.y);

namespace Catch
{
template <> struct StringMaker<rive::Mat2D>
{
    static std::string convert(rive::Mat2D const& value)
    {
        std::ostringstream os;
        os << value[0] << ", " << value[1] << ", " << value[2] << ", "
           << value[3] << ", " << value[4] << ", " << value[5];
        return os.str();
    }
};
template <> struct StringMaker<rive::AABB>
{
    static std::string convert(rive::AABB const& value)
    {
        std::ostringstream os;
        os << "min: " << value.minX << ", " << value.minY
           << " max: " << value.maxX << ", " << value.maxY;
        return os.str();
    }
};
#ifdef WITH_RIVE_TEXT
template <> struct StringMaker<rive::CursorPosition>
{
    static std::string convert(rive::CursorPosition const& value)
    {
        std::ostringstream os;
        os << "Line: " << value.lineIndex()
           << ", CodePointIndex: " << value.codePointIndex();
        return os.str();
    }
};
#endif
} // namespace Catch
#endif