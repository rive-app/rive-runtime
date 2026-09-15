#ifndef _RIVE_KEYED_OBJECT_HPP_
#define _RIVE_KEYED_OBJECT_HPP_
#include "rive/generated/animation/keyed_object_base.hpp"
#include <vector>
namespace rive
{
class Artboard;
class KeyedProperty;
class KeyedCallbackReporter;
class LinearAnimationInstance;
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
                              bool isAtStartFrame) const;
    void apply(Artboard* coreContext,
               float time,
               float mix,
               const LinearAnimationInstance* context = nullptr);

    StatusCode import(ImportStack& importStack) override;

    const KeyedProperty* getProperty(size_t index) const
    {
        if (index < m_keyedProperties.size())
        {
            return m_keyedProperties[index].get();
        }
        else
        {
            return nullptr;
        }
    }

    size_t numKeyedProperties() const { return m_keyedProperties.size(); }

#ifdef WITH_RIVE_EDITOR
    // Editor-only: parallel non-owning list populated by
    // `EditorFile::finalizeBatch` from coop-hydrated KeyedProperties
    // whose `keyedObjectId` resolves to this KeyedObject. See
    // `LinearAnimation::m_EditorKeyedObjects` for the dual-mode
    // rationale.
    void addKeyedPropertyForEditor(KeyedProperty* property);
    void clearEditorKeyedProperties();
    /// Read-only view for the timeline FFI.
    const std::vector<KeyedProperty*>& editorKeyedProperties() const
    {
        return m_editorKeyedProperties;
    }
#endif

private:
    std::vector<std::unique_ptr<KeyedProperty>> m_keyedProperties;
#ifdef WITH_RIVE_EDITOR
    std::vector<KeyedProperty*> m_editorKeyedProperties;
#endif
};
} // namespace rive

#endif