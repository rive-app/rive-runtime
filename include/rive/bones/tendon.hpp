#ifndef _RIVE_TENDON_HPP_
#define _RIVE_TENDON_HPP_

#include "rive/generated/bones/tendon_base.hpp"
#include "rive/math/mat2d.hpp"
#include <stdio.h>

namespace rive
{
class Bone;
class Tendon : public TendonBase
{
private:
    Mat2D m_InverseBind;
    Bone* m_Bone = nullptr;

public:
    Bone* bone() const { return m_Bone; }
    const Mat2D& inverseBind() const { return m_InverseBind; }
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
};
} // namespace rive

#endif