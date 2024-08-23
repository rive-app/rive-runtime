/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_CGSkiaFactory_HPP_
#define _RIVE_CGSkiaFactory_HPP_

#include "skia_factory.hpp"

namespace rive
{
struct CGSkiaFactory : public SkiaFactory
{
    std::vector<uint8_t> platformDecode(Span<const uint8_t>, SkiaFactory::ImageInfo*) override;
};
} // namespace rive

#endif // _RIVE_CGSkiaFactory_HPP_
