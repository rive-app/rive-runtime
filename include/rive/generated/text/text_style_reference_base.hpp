#ifndef _RIVE_TEXT_STYLE_REFERENCE_BASE_HPP_
#define _RIVE_TEXT_STYLE_REFERENCE_BASE_HPP_
#include "rive/container_component.hpp"
namespace rive
{
class TextStyleReferenceBase : public ContainerComponent
{
protected:
  typedef ContainerComponent Super;

public:
  static const uint16_t typeKey = 1100;

  uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif