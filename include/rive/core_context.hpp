#ifndef _RIVE_CORE_CONTEXT_HPP_
#define _RIVE_CORE_CONTEXT_HPP_

#include "rive/rive_types.hpp"

namespace rive
{
class Artboard;
class Core;
class CoreContext
{
public:
    virtual ~CoreContext() {}
    virtual Core* resolve(uint32_t id) const = 0;
};
} // namespace rive
#endif
