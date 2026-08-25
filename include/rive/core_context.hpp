#ifndef _RIVE_CORE_CONTEXT_HPP_
#define _RIVE_CORE_CONTEXT_HPP_

#include "rive/core/id.hpp"
#include "rive/rive_types.hpp"

namespace rive
{
class Artboard;
class Core;
class CoreContext
{
public:
    virtual ~CoreContext() {}
    // Resolve an object reference to the actual Core*. The type of
    // `id` is `uint32_t` in runtime-only builds (flat index into the
    // artboard's object list) and a `{client, object}` struct in
    // editor builds — see `rive/core/id.hpp` for the ifdef.
    virtual Core* resolve(Id id) const = 0;
};
} // namespace rive
#endif
