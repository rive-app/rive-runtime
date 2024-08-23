#include "rive/bones/weight.hpp"
#include "rive/container_component.hpp"
#include "rive/shapes/vertex.hpp"

using namespace rive;

StatusCode Weight::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    if (!parent()->is<Vertex>())
    {
        return StatusCode::MissingObject;
    }

    parent()->as<Vertex>()->weight(this);

    return StatusCode::Ok;
}

static int encodedWeightValue(unsigned int index, unsigned int data)
{
    return (data >> (index * 8)) & 0xFF;
}

Vec2D Weight::deform(Vec2D inPoint,
                     unsigned int indices,
                     unsigned int weights,
                     const Mat2D& world,
                     const float* boneTransforms)
{
    float xx = 0, xy = 0, yx = 0, yy = 0, tx = 0, ty = 0;
    for (int i = 0; i < 4; i++)
    {
        int weight = encodedWeightValue(i, weights);
        if (weight == 0)
        {
            continue;
        }

        float normalizedWeight = weight / 255.0f;
        int index = encodedWeightValue(i, indices);
        int startBoneTransformIndex = index * 6;
        xx += boneTransforms[startBoneTransformIndex++] * normalizedWeight;
        xy += boneTransforms[startBoneTransformIndex++] * normalizedWeight;
        yx += boneTransforms[startBoneTransformIndex++] * normalizedWeight;
        yy += boneTransforms[startBoneTransformIndex++] * normalizedWeight;
        tx += boneTransforms[startBoneTransformIndex++] * normalizedWeight;
        ty += boneTransforms[startBoneTransformIndex++] * normalizedWeight;
    }

    return Mat2D(xx, xy, yx, yy, tx, ty) * (world * inPoint);
}
