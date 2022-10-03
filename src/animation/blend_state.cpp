#include "rive/animation/blend_state.hpp"
#include "rive/animation/blend_animation.hpp"

using namespace rive;

BlendState::~BlendState()
{
    for (auto anim : m_Animations)
    {
        delete anim;
    }
}

void BlendState::addAnimation(BlendAnimation* animation)
{
    // Assert it's not already contained.
    assert(std::find(m_Animations.begin(), m_Animations.end(), animation) == m_Animations.end());
    m_Animations.push_back(animation);
}