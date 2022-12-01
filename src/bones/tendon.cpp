#include "rive/bones/tendon.hpp"
#include "rive/bones/bone.hpp"
#include "rive/bones/skin.hpp"
#include "rive/core_context.hpp"

using namespace rive;

StatusCode Tendon::onAddedDirty(CoreContext* context)
{
    Mat2D bind;
    bind[0] = xx();
    bind[1] = xy();
    bind[2] = yx();
    bind[3] = yy();
    bind[4] = tx();
    bind[5] = ty();

    if (!bind.invert(&m_InverseBind))
    {
        return StatusCode::FailedInversion;
    }

    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto coreObject = context->resolve(boneId());
    if (coreObject == nullptr || !coreObject->is<Bone>())
    {
        return StatusCode::MissingObject;
    }

    m_Bone = static_cast<Bone*>(coreObject);

    return StatusCode::Ok;
}

StatusCode Tendon::onAddedClean(CoreContext* context)
{
    if (!parent()->is<Skin>())
    {
        return StatusCode::MissingObject;
    }

    parent()->as<Skin>()->addTendon(this);

    return StatusCode::Ok;
}
