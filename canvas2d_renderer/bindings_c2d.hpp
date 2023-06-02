#include "rive/rive_types.hpp"

namespace rive
{
class Factory;
class Renderer;
std::unique_ptr<rive::Factory> MakeC2DFactory();
std::unique_ptr<rive::Renderer> MakeC2DRenderer(const char* canvasID);
void ClearC2DRenderer(rive::Renderer*);
} // namespace rive
