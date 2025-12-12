#ifndef _RIVE_FOCUSABLE_HPP_
#define _RIVE_FOCUSABLE_HPP_

#include "rive/enum_bitset.hpp"

namespace rive
{
enum class KeyModifiers : uint8_t
{
    none = 0,
    shift = 1 << 0,
    ctrl = 1 << 1,
    alt = 1 << 2,
    meta = 1 << 3

};

inline KeyModifiers operator|(const KeyModifiers& a, const KeyModifiers& b)
{
    return (KeyModifiers)((uint8_t)a | (uint8_t)b);
}
inline KeyModifiers operator&(const KeyModifiers& a, const KeyModifiers& b)
{
    return (KeyModifiers)((uint8_t)a & (uint8_t)b);
}

enum class Key : uint16_t
{
    space = 32,
    apostrophe = 39, // '
    comma = 44,      // ,
    minus = 45,      // -
    period = 46,     // .
    slash = 47,      // /
    key0 = 48,
    key1 = 49,
    key2 = 50,
    key3 = 51,
    key4 = 52,
    key5 = 53,
    key6 = 54,
    key7 = 55,
    key8 = 56,
    key9 = 57,
    semicolon = 59, // ;
    equal = 61,     // =
    a = 65,
    b = 66,
    c = 67,
    d = 68,
    e = 69,
    f = 70,
    g = 71,
    h = 72,
    i = 73,
    j = 74,
    k = 75,
    l = 76,
    m = 77,
    n = 78,
    o = 79,
    p = 80,
    q = 81,
    r = 82,
    s = 83,
    t = 84,
    u = 85,
    v = 86,
    w = 87,
    x = 88,
    y = 89,
    z = 90,
    leftBracket = 91,  // [
    backslash = 92,    // "\"
    rightBracket = 93, // ]
    graveAccent = 96,  // `
    world1 = 161,      // non-US #1
    world2 = 162,      // non-US #2
    escape = 256,
    enter = 257,
    tab = 258,
    backspace = 259,
    insert = 260,
    deleteKey = 261,
    right = 262,
    left = 263,
    down = 264,
    up = 265,
    pageUp = 266,
    pageDown = 267,
    home = 268,
    end = 269,
    capsLock = 280,
    scrollLock = 281,
    numLock = 282,
    printScreen = 283,
    pause = 284,
    f1 = 290,
    f2 = 291,
    f3 = 292,
    f4 = 293,
    f5 = 294,
    f6 = 295,
    f7 = 296,
    f8 = 297,
    f9 = 298,
    f10 = 299,
    f11 = 300,
    f12 = 301,
    f13 = 302,
    f14 = 303,
    f15 = 304,
    f16 = 305,
    f17 = 306,
    f18 = 307,
    f19 = 308,
    f20 = 309,
    f21 = 310,
    f22 = 311,
    f23 = 312,
    f24 = 313,
    f25 = 314,
    kp0 = 320,
    kp1 = 321,
    kp2 = 322,
    kp3 = 323,
    kp4 = 324,
    kp5 = 325,
    kp6 = 326,
    kp7 = 327,
    kp8 = 328,
    kp9 = 329,
    kpDecimal = 330,
    kpDivide = 331,
    kpMultiply = 332,
    kpSubtract = 333,
    kpAdd = 334,
    kpEnter = 335,
    kpEqual = 336,
    leftShift = 340,
    leftControl = 341,
    leftAlt = 342,
    leftSuper = 343,
    rightShift = 344,
    rightControl = 345,
    rightAlt = 346,
    rightSuper = 347,
    menu = 348,
};

class Focusable
{
public:
    virtual bool keyInput(Key value,
                          KeyModifiers modifiers,
                          bool isPressed,
                          bool isRepeat) = 0;
    virtual bool textInput(const std::string& text) = 0;
};
} // namespace rive
#endif