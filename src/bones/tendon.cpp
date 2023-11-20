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

    // Internally, this inversion can fail because of a division by 0.
    // 'm_InverseBind' will default to an identity matrix. Although
    // this can be treated as undefined behavior, the editor lets it go
    // through, and nothing really breaks, it just can look odd. So we
    // allow the runtime to behave in the same way and return StatusCode::Ok.
    bind.invert(&m_InverseBind);

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
