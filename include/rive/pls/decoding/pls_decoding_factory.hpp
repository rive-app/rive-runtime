/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_factory.hpp"

namespace rive::pls
{
class PLSDecodingFactory : public PLSFactory
{
public:
    std::unique_ptr<RenderImage> decodeImage(Span<const uint8_t>) override;
};
} // namespace rive::pls
