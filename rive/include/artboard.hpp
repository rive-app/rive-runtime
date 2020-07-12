#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_
#include "generated/artboard_base.hpp"
#include <vector>
namespace rive
{
	class Artboard : public ArtboardBase
	{
	private:
		std::vector<Core*> m_Objects;

	public:
		void addObject(Core* object);
	};
} // namespace rive

#endif