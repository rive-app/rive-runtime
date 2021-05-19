#include "animation/blend_state.hpp"

using namespace rive;

void BlendState::addAnimation(BlendAnimation* animation)
{
	// Assert it's not already contained.
	assert(std::find(m_Animations.begin(), m_Animations.end(), animation) ==
	       m_Animations.end());
	m_Animations.push_back(animation);
}