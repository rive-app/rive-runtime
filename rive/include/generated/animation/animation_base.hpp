#ifndef _RIVE_ANIMATION_BASE_HPP_
#define _RIVE_ANIMATION_BASE_HPP_
#include "core.hpp"
#include <string>
namespace rive
{
	class AnimationBase : public Core
	{
	private:
		std::string m_Name;
	public:
		std::string name() const { return m_Name; }
		void name(std::string value) { m_Name = value; }
	};
} // namespace rive

#endif