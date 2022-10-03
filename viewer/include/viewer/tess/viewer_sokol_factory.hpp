#ifndef _RIVE_VIEWER_SOKOL_FACTORY_HPP_
#define _RIVE_VIEWER_SOKOL_FACTORY_HPP_

#include "rive/tess/sokol/sokol_factory.hpp"

class ViewerSokolFactory : public rive::SokolFactory
{
public:
    std::unique_ptr<rive::RenderImage> decodeImage(rive::Span<const uint8_t>) override;
};
#endif