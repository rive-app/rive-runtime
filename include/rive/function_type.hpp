#ifndef _RIVE_FUNCTION_TYPE_HPP_
#define _RIVE_FUNCTION_TYPE_HPP_

namespace rive
{
enum class FunctionType : int
{
    min = 0,
    max = 1,
    round = 2,
    ceil = 3,
    floor = 4,
    sqrt = 5,
    pow = 6,
    exp = 7,
    log = 8,
    cosine = 9,
    sine = 10,
    tangent = 11,
    acosine = 12,
    asine = 13,
    atangent = 14,
    atangent2 = 15,
    random = 16,
};
} // namespace rive

#endif
