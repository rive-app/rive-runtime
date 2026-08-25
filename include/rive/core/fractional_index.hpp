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

    // Cross-multiply compare: a/b vs c/d → a*d vs c*b. Dart does this
    // in wrapping 64 bit ints, so the product difference is taken
    // unsigned and read back as signed to wrap the same way.
    int compareTo(const FractionalIndex& other) const
    {
        const uint64_t difference =
            static_cast<uint64_t>(numerator) *
                static_cast<uint64_t>(other.denominator) -
            static_cast<uint64_t>(denominator) *
                static_cast<uint64_t>(other.numerator);
        const int64_t signedDifference = static_cast<int64_t>(difference);
        return signedDifference < 0 ? -1 : (signedDifference > 0 ? 1 : 0);
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
