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

    static Skinnable* from(Component* component);
};
} // namespace rive

#endif