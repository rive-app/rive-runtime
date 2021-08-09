#include "rive/bones/weight.hpp"
#include "rive/container_component.hpp"
#include "rive/shapes/path_vertex.hpp"

using namespace rive;

StatusCode Weight::onAddedDirty(CoreContext* context)
{
	StatusCode code = Super::onAddedDirty(context);
	if (code != StatusCode::Ok)
	{
		return code;
	}
	if (!parent()->is<PathVertex>())
	{
		return StatusCode::MissingObject;
	}

	parent()->as<PathVertex>()->weight(this);

	return StatusCode::Ok;
}

static int encodedWeightValue(unsigned int index, unsigned int data)
{
	return (data >> (index * 8)) & 0xFF;
}

void Weight::deform(float x,
                    float y,
                    unsigned int indices,
                    unsigned int weights,
                    const Mat2D& world,
                    const float* boneTransforms,
                    Vec2D& result)
{
	float xx = 0, xy = 0, yx = 0, yy = 0, tx = 0, ty = 0;
	float rx = world[0] * x + world[2] * y + world[4];
	float ry = world[1] * x + world[3] * y + world[5];
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
	result[0] = xx * rx + yx * ry + tx;
	result[1] = xy * rx + yy * ry + ty;
}