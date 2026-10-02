#ifndef _RIVE_TRANSITION_CHILD_REF_HPP_
#define _RIVE_TRANSITION_CHILD_REF_HPP_

#include "rive/artboard.hpp"
#include "rive/renderer.hpp"
#include "rive/scripted/script_backend.hpp"

namespace rive
{
// drawInternal ignores isHidden, so a child kept out of the normal draw loop
// still renders here.
inline void ScriptBackend::TransitionChildRef::draw(Renderer* renderer) const
{
    if (artboard == nullptr)
    {
        return;
    }
    renderer->save();
    renderer->transform(transform);
    artboard->drawInternal(renderer);
    renderer->restore();
}

inline float ScriptBackend::TransitionChildRef::width() const
{
    return artboard != nullptr ? artboard->width() : 0.0f;
}

inline float ScriptBackend::TransitionChildRef::height() const
{
    return artboard != nullptr ? artboard->height() : 0.0f;
}
} // namespace rive

#endif
