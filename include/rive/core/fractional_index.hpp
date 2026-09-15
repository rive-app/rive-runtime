#ifndef _RIVE_FRACTIONAL_INDEX_HPP_
#define _RIVE_FRACTIONAL_INDEX_HPP_

#ifdef WITH_RIVE_EDITOR

#include <cstdint>

namespace rive
{
// Editor-only sibling-order key. Mirrors Dart's `FractionalIndex`
// (packages/fractional/lib/fractional.dart): a `(numerator,
// denominator)` rational that supports arbitrary insertion between
// existing keys without renumbering. Cross-multiplication ordering
// avoids float division.
//
// Both fields are 64 bit because Dart's are: an index that fits Dart's
// int has to survive the round trip through here, and a mediant
// ladder has to be able to grow the way Dart's does.
//
// Wire format: two varuints, in `(numerator, denominator)` order —
// matches `CoreFractionalIndexType::serialize`. The "invalid" default
// `{-1, 0}` is the sentinel for "no order assigned yet"; it compares
// less than any valid index but equal to other invalids.
struct FractionalIndex
{
    int64_t numerator = -1;
    int64_t denominator = 0;

    constexpr FractionalIndex() = default;
    constexpr FractionalIndex(int64_t n, int64_t d) :
        numerator(n), denominator(d)
    {}

    static constexpr FractionalIndex invalid() { return {-1, 0}; }

    // Boundary sentinels used by the editor's "insert between" logic
    // when one side has no neighbor (start/end of an open path). These
    // are Dart's `FractionalIndex.min` / `FractionalIndex.max`
    // verbatim: a `max()` of anything else walks a different mediant
    // ladder than Dart does from the same starting point, so the two
    // editors disagree about a list they both appended to.
    static constexpr FractionalIndex min() { return {0, 1}; }
    static constexpr FractionalIndex max() { return {1, 1}; }

    bool isValid() const { return denominator != 0; }

    // Mediant (Stern-Brocot) of two indices, GCD-reduced so num/den
    // don't blow up under repeated inserts. Returns a fraction
    // strictly between `a` and `b` whenever they're ordered. Both
    // operands must be valid (non-invalid); callers are expected to
    // substitute `min()` / `max()` for missing neighbors.
    static FractionalIndex between(const FractionalIndex& a,
                                   const FractionalIndex& b)
    {
        const int64_t num = a.numerator + b.numerator;
        const int64_t den = a.denominator + b.denominator;
        // GCD on absolute values; dart's `%` is euclidean so its reduce
        // converges on the same positive divisor.
        int64_t x = num < 0 ? -num : num;
        int64_t y = den < 0 ? -den : den;
        while (y != 0)
        {
            const int64_t t = y;
            y = x % y;
            x = t;
        }
        const int64_t g = x == 0 ? 1 : x;
        return FractionalIndex{num / g, den / g};
    }

    // Append past the end without an upper bound — strictly greater
    // than this index. Mirrors dart FractionalIndex.nextUnbounded.
    FractionalIndex nextUnbounded() const
    {
        return {numerator + 1, denominator};
    }

    // Strictly smaller valid index (mediant toward min()).
    FractionalIndex prev() const { return between(*this, min()); }

    // Cross-multiply compare: a/b vs c/d → a*d vs c*b, exact in 128
    // bits like dart's BigInt path on web. A wrapping 64 bit product
    // goes non-transitive once a mediant ladder grows past 2^32 and
    // scrambles any sort over the list.
    int compareTo(const FractionalIndex& other) const
    {
        const int leftSign = productSign(numerator, other.denominator);
        const int rightSign = productSign(other.numerator, denominator);
        if (leftSign != rightSign)
        {
            return leftSign < rightSign ? -1 : 1;
        }
        if (leftSign == 0)
        {
            return 0;
        }
        uint64_t leftHi, leftLo, rightHi, rightLo;
        mulAbs(numerator, other.denominator, leftHi, leftLo);
        mulAbs(other.numerator, denominator, rightHi, rightLo);
        int magnitude = 0;
        if (leftHi != rightHi)
        {
            magnitude = leftHi < rightHi ? -1 : 1;
        }
        else if (leftLo != rightLo)
        {
            magnitude = leftLo < rightLo ? -1 : 1;
        }
        return leftSign > 0 ? magnitude : -magnitude;
    }

    static int productSign(int64_t x, int64_t y)
    {
        if (x == 0 || y == 0)
        {
            return 0;
        }
        return (x < 0) == (y < 0) ? 1 : -1;
    }

    // |x| * |y| as a 128 bit value in two 64 bit halves.
    static void mulAbs(int64_t x, int64_t y, uint64_t& hi, uint64_t& lo)
    {
        const uint64_t ux =
            x < 0 ? ~static_cast<uint64_t>(x) + 1 : static_cast<uint64_t>(x);
        const uint64_t uy =
            y < 0 ? ~static_cast<uint64_t>(y) + 1 : static_cast<uint64_t>(y);
#if defined(__SIZEOF_INT128__)
        const unsigned __int128 product =
            static_cast<unsigned __int128>(ux) * uy;
        hi = static_cast<uint64_t>(product >> 64);
        lo = static_cast<uint64_t>(product);
#else
        const uint64_t xl = ux & 0xffffffffu;
        const uint64_t xh = ux >> 32;
        const uint64_t yl = uy & 0xffffffffu;
        const uint64_t yh = uy >> 32;
        const uint64_t ll = xl * yl;
        const uint64_t mid = xl * yh + (ll >> 32);
        const uint64_t mid2 = xh * yl + (mid & 0xffffffffu);
        lo = (ll & 0xffffffffu) | (mid2 << 32);
        hi = xh * yh + (mid >> 32) + (mid2 >> 32);
#endif
    }

    friend constexpr bool operator==(const FractionalIndex& a,
                                     const FractionalIndex& b)
    {
        return a.numerator == b.numerator && a.denominator == b.denominator;
    }
    friend constexpr bool operator!=(const FractionalIndex& a,
                                     const FractionalIndex& b)
    {
        return !(a == b);
    }
    friend bool operator<(const FractionalIndex& a, const FractionalIndex& b)
    {
        return a.compareTo(b) < 0;
    }
    friend bool operator>(const FractionalIndex& a, const FractionalIndex& b)
    {
        return a.compareTo(b) > 0;
    }
};
} // namespace rive

#endif
#endif
