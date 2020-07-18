#ifndef _RIVE_LINEAR_ANIMATION_HPP_
#define _RIVE_LINEAR_ANIMATION_HPP_
#include "generated/animation/linear_animation_base.hpp"
#include <vector>
namespace rive
{
	class KeyedObject;
	class LinearAnimation : public LinearAnimationBase
	{
	private:
		std::vector<KeyedObject*> m_KeyedObjects;
	public:
		~LinearAnimation();
		void addKeyedObject(KeyedObject* object);
	};
} // namespace rive

#endif