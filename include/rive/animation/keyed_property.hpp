#ifndef _RIVE_KEYED_PROPERTY_HPP_
#define _RIVE_KEYED_PROPERTY_HPP_
#include "rive/generated/animation/keyed_property_base.hpp"
#include <vector>
namespace rive {
    class KeyFrame;
    class KeyedProperty : public KeyedPropertyBase {
    private:
        std::vector<KeyFrame*> m_KeyFrames;

    public:
        ~KeyedProperty();
        void addKeyFrame(KeyFrame* keyframe);
        StatusCode onAddedClean(CoreContext* context) override;
        StatusCode onAddedDirty(CoreContext* context) override;

        void apply(Core* object, float time, float mix);

        StatusCode import(ImportStack& importStack) override;
    };
} // namespace rive

#endif