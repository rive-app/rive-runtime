#ifndef _RIVE_TRANSITION_VALUE_ASSET_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_VALUE_ASSET_COMPARATOR_BASE_HPP_
#include "rive/animation/transition_value_id_comparator.hpp"
namespace rive
{
class TransitionValueAssetComparatorBase : public TransitionValueIdComparator
{
protected:
    typedef TransitionValueIdComparator Super;

public:
    static const uint16_t typeKey = 602;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif