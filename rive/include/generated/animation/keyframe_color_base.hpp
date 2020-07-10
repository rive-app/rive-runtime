#ifndef _RIVE_KEY_FRAME_COLOR_BASE_HPP_
#define _RIVE_KEY_FRAME_COLOR_BASE_HPP_
#include "animation/keyframe.hpp"
namespace rive
{
	class KeyFrameColorBase : public KeyFrame
	{
	private:
		int m_Value = 0;
	public:
		int value() const { return m_Value; }
		void value(int value) { m_Value = value; }
	};
} // namespace rive

#endif