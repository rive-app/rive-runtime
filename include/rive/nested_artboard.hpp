#ifndef _RIVE_NESTED_ARTBOARD_HPP_
#define _RIVE_NESTED_ARTBOARD_HPP_
#include "rive/generated/nested_artboard_base.hpp"
#include <stdio.h>
namespace rive
{
	class NestedArtboard : public NestedArtboardBase
	{

	private:
		Artboard* m_NestedInstance = nullptr;

	public:
		~NestedArtboard();
		void draw(Renderer* renderer) override;

		void nest(Artboard* artboard);

		StatusCode import(ImportStack& importStack) override;
		Core* clone() const override;
	};
} // namespace rive

#endif