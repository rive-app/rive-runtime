#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_
#include "core_context.hpp"
#include "generated/artboard_base.hpp"

#include <vector>
namespace rive
{
	class Animation;
	class File;
	class Artboard : public ArtboardBase, public CoreContext
	{
		friend class File;

	private:
		std::vector<Core*> m_Objects;
		std::vector<Animation*> m_Animations;

		void initialize();

	public:
		~Artboard();
		void addObject(Core* object);
		void addAnimation(Animation* object);

		Core* resolve(int id) const override;

		void onAddedClean(CoreContext* context) override {}

		Core* find(std::string name);
	};
} // namespace rive

#endif