#include "rive/joystick.hpp"
#include "rive/artboard.hpp"

using namespace rive;

StatusCode Joystick::onAddedClean(CoreContext* context)
{
    m_xAnimation = artboard()->animation(xId());
    m_yAnimation = artboard()->animation(yId());
    return StatusCode::Ok;
}

void Joystick::apply(Artboard* artboard) const
{
    if (m_xAnimation != nullptr)
    {
        m_xAnimation->apply(artboard, (x() + 1.0f) / 2.0f * m_xAnimation->durationSeconds());
    }
    if (m_yAnimation != nullptr)
    {
        m_yAnimation->apply(artboard, (y() + 1.0f) / 2.0f * m_yAnimation->durationSeconds());
    }
}
