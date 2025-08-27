#ifndef _RIVE_ARITHMETIC_OPERATION_HPP_
#define _RIVE_ARITHMETIC_OPERATION_HPP_

namespace rive
{
enum class ArithmeticOperation : int
{
    add = 0,
    subtract = 1,
    multiply = 2,
    divide = 3,
    modulo = 4,
    squareRoot = 5,
    power = 6,
    exp = 7,
    log = 8,
    cosine = 9,
    sine = 10,
    tangent = 11,
    acosine = 12,
    asine = 13,
    atangent = 14,
    atangent2 = 15,
    round = 16,
    floor = 17,
    ceil = 18,
};
} // namespace rive

#endif