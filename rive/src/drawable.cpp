#include "drawable.hpp"
#include "artboard.hpp"

using namespace rive;

void Drawable::drawOrderChanged()
{
	artboard()->addDirt(ComponentDirt::DrawOrder);
}