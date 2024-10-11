#include "rive/math/path_measure.hpp"

using namespace rive;

PathMeasure::PathMeasure() : m_length(0.0f) {}

PathMeasure::PathMeasure(const RawPath* path) : m_length(0.0f)
{
    auto measure = ContourMeasureIter(path);
    for (auto contour = measure.next(); contour != nullptr;
         contour = measure.next())
    {
        m_length += contour->length();
        m_contours.push_back(contour);
    }
}

ContourMeasure::PosTanDistance PathMeasure::atDistance(float distance) const
{
    float currentDistance = distance;
    for (auto contour : m_contours)
    {
        float contourLength = contour->length();
        if (currentDistance - contourLength <= 0)
        {
            return ContourMeasure::PosTanDistance(
                contour->getPosTan(currentDistance),
                distance);
        }
        currentDistance -= contourLength;
    }
    return ContourMeasure::PosTanDistance();
}

ContourMeasure::PosTanDistance PathMeasure::atPercentage(
    float percentageDistance) const
{
    float inRangePercentage = fmodf(percentageDistance, 1.0f);
    if (inRangePercentage < 0.0f)
    {
        inRangePercentage += 1.0f;
    }

    // Mod to correct percentage (0-100%) and make sure we actually reach
    // 100%.
    if (percentageDistance != 0.0f && inRangePercentage == 0.0f)
    {
        inRangePercentage = 1.0f;
    }

    return atDistance(m_length * inRangePercentage);
}
