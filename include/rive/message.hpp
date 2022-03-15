#ifndef _RIVE_MESSAGE_HPP_
#define _RIVE_MESSAGE_HPP_

#include "rive/math/vec2d.hpp"
#include <string>

namespace rive {

struct Message {
    // TODO -- how to represent this?
    // Perhaps some sort of JSON-like object: key-value pairs?
    // For now, store a string so we can test it...
    
    std::string m_Str;
};
                              
} // namespace rive

#endif
