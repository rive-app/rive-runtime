#ifndef _RIVE_SKINNABLE_HPP_
#define _RIVE_SKINNABLE_HPP_

#include "rive/rive_types.hpp"

namespace rive
{
class Skin;
class Component;

class Skinnable
{
    friend class Skin;

private:
    Skin* m_Skin = nullptr;

protected:
    void skin(Skin* skin);

public:
    virtual ~Skinnable() {}

    Skin* skin() const { return m_Skin; }
    virtual void markSkinDirty() = 0;

#ifdef WITH_RIVE_EDITOR
    /// Weights, tendons or the skin itself changed, unlike markSkinDirty
    /// which also fires for every bone move.
    virtual void bindingChangedForEditor() {}
    /// Skin::editorParentChanged calls these on parent transitions.
    void setSkinForEditor(Skin* s)
    {
        m_Skin = s;
        bindingChangedForEditor();
    }
    void clearSkinIfForEditor(Skin* expected)
    {
        if (m_Skin == expected)
        {
            m_Skin = nullptr;
            bindingChangedForEditor();
        }
    }
#endif

    static Skinnable* from(Component* component);
};
} // namespace rive

#endif