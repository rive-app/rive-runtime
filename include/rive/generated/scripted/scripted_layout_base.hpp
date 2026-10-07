#ifndef _RIVE_SCRIPTED_LAYOUT_BASE_HPP_
#define _RIVE_SCRIPTED_LAYOUT_BASE_HPP_
#include "rive/scripted/scripted_drawable.hpp"
namespace rive
{
class ScriptedLayoutBase : public ScriptedDrawable
{
protected:
    typedef ScriptedDrawable Super;

public:
    static const uint16_t typeKey = 637;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif