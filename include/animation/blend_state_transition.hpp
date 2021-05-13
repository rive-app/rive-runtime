#ifndef _RIVE_BLEND_STATE_TRANSITION_HPP_
#define _RIVE_BLEND_STATE_TRANSITION_HPP_
#include "generated/animation/blend_state_transition_base.hpp"
#include <stdio.h>
namespace rive
{
	class BlendAnimation;
	class LayerStateImporter;
	class BlendStateTransition : public BlendStateTransitionBase
	{
		friend class LayerStateImporter;

	private:
		BlendAnimation* m_ExitBlendAnimation = nullptr;

	public:
		BlendAnimation* exitBlendAnimation() const
		{
			return m_ExitBlendAnimation;
		}

		StatusCode onAddedClean(CoreContext* context);
	};

} // namespace rive

#endif