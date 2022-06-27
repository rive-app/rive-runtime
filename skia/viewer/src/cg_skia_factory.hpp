/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_CGSkiaFactory_HPP_
#define _RIVE_CGSkiaFactory_HPP_

#include "skia_factory.hpp"

namespace rive {

struct CGSkiaFactory : public SkiaFactory {
    std::vector<uint8_t> platformDecode(rive::Span<const uint8_t> span, ImageInfo* info) override;
};

} // namespace

#endif
