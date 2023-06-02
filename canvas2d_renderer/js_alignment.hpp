#ifndef _RIVE_JS_ALIGNMENT_HPP_
#define _RIVE_JS_ALIGNMENT_HPP_

#include "rive/layout.hpp"

enum class JsAlignment : unsigned char
{
    topLeft,
    topCenter,
    topRight,
    centerLeft,
    center,
    centerRight,
    bottomLeft,
    bottomCenter,
    bottomRight
};

rive::Alignment convertAlignment(JsAlignment alignment);
#endif