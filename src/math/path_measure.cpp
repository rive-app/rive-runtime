#include "rive/math/path_measure.hpp"
#include <algorithm>

using namespace rive;

PathMeasure::PathMeasure() : m_length(0.0f) {}

PathMeasure::PathMeasure(const RawPath* path, float tol) : m_length(0.0f)
{
    auto measure = ContourMeasureIter(path, tol);
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

void PathMeasure::getSegment(float startDistance,
                             float endDistance,
                             RawPath* dst,
                             bool startWithMove) const
{
    if (dst == nullptr || m_contours.empty())
    {
        return;
    }

    // Clamp distances to valid range
    startDistance = std::max(0.0f, std::min(startDistance, m_length));
    endDistance = std::max(0.0f, std::min(endDistance, m_length));

    if (startDistance >= endDistance)
    {
        return;
    }

    float currentDistance = 0.0f;
    bool isFirstSegment = true;

    for (auto contour : m_contours)
    {
        float contourLength = contour->length();
        float contourStart = currentDistance;
        float contourEnd = currentDistance + contourLength;

        // Check if this contour intersects with the requested range
        if (contourEnd > startDistance && contourStart < endDistance)
        {
            // Calculate the local distances within this contour
            float localStart = std::max(0.0f, startDistance - contourStart);
            float localEnd =
                std::min(contourLength, endDistance - contourStart);

            // Extract from this contour
            contour->getSegment(localStart,
                                localEnd,
                                dst,
                                !isFirstSegment || startWithMove);
            isFirstSegment = false;
        }

        currentDistance += contourLength;

        // If we've passed the end distance, we're done
        if (currentDistance >= endDistance)
        {
            break;
        }
    }
}

bool PathMeasure::isClosed() const
{
    // Return true only if there is exactly one contour and it is closed
    return m_contours.size() == 1 && m_contours[0]->isClosed();
}
