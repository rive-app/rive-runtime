/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/refcnt.hpp"

namespace rive::pls
{
// Wraps a backend-specific buffer that PLSRenderContext draws into.
class PLSRenderTarget : public RefCnt<PLSRenderTarget>
{
public:
    virtual ~PLSRenderTarget() {}

    size_t width() const { return m_width; }
    size_t height() const { return m_height; }

protected:
    PLSRenderTarget(size_t width, size_t height) : m_width(width), m_height(height) {}

private:
    size_t m_width;
    size_t m_height;
};
} // namespace rive::pls
