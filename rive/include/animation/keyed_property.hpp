#ifndef _RIVE_KEYED_PROPERTY_HPP_
#define _RIVE_KEYED_PROPERTY_HPP_
#include "generated/animation/keyed_property_base.hpp"
#include <vector>
namespace rive
{
	class KeyFrame;
	class KeyedProperty : public KeyedPropertyBase
	{
	private:
		std::vector<KeyFrame*> m_KeyFrames;
	public:
		~KeyedProperty();
		void addKeyFrame(KeyFrame* keyframe);
		void onAddedClean(CoreContext* context) {}
		void onAddedDirty(CoreContext* context) {}
	};
} // namespace rive

#endif