#ifndef _RIVE_KEYED_OBJECT_HPP_
#define _RIVE_KEYED_OBJECT_HPP_
#include "rive/generated/animation/keyed_object_base.hpp"
#include <vector>
namespace rive
{
class Artboard;
class KeyedProperty;
class KeyedCallbackReporter;
class KeyedObject : public KeyedObjectBase
{
public:
    KeyedObject();
    ~KeyedObject() override;
    void addKeyedProperty(std::unique_ptr<KeyedProperty>);

    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
    void reportKeyedCallbacks(KeyedCallbackReporter* reporter,
                              float secondsFrom,
                              float secondsTo,
                              int secondsFromExactOffset) const;
    void apply(Artboard* coreContext, float time, float mix);

    StatusCode import(ImportStack& importStack) override;

private:
    std::vector<std::unique_ptr<KeyedProperty>> m_keyedProperties;
};
} // namespace rive

#endif