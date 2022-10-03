#ifndef _RIVE_KEYED_OBJECT_HPP_
#define _RIVE_KEYED_OBJECT_HPP_
#include "rive/generated/animation/keyed_object_base.hpp"
#include <vector>
namespace rive
{
class Artboard;
class KeyedProperty;
class KeyedObject : public KeyedObjectBase
{
private:
    std::vector<std::unique_ptr<KeyedProperty>> m_KeyedProperties;

public:
    KeyedObject();
    ~KeyedObject() override;
    void addKeyedProperty(std::unique_ptr<KeyedProperty>);

    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
    void apply(Artboard* coreContext, float time, float mix);

    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif