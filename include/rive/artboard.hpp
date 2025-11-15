#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_

#include "rive/advance_flags.hpp"
#include "rive/resetting_component.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/core_context.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/data_bind/data_bind_container.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/hit_info.hpp"
#include "rive/math/aabb.hpp"
#include "rive/renderer.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/event.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/typed_children.hpp"
#include "rive/virtualizing_component.hpp"

#include <queue>
#include <unordered_set>
#include <vector>

namespace rive
{
class ArtboardComponentList;
class ArtboardHost;
class File;
class Drawable;
class Factory;
class Node;
class DrawTarget;
class ArtboardImporter;
class NestedArtboard;
class ArtboardInstance;
class LinearAnimationInstance;
class Scene;
class StateMachineInstance;
class Joystick;
class TextValueRun;
class Event;
class SMIBool;
class SMIInput;
class SMINumber;
class SMITrigger;
class DataBind;
class DataBindContainer;
class ScriptedObject;

#ifdef WITH_RIVE_TOOLS
typedef void (*ArtboardCallback)(void*);
typedef uint8_t (*TestBoundsCallback)(void*, float, float, bool);
typedef uint8_t (*IsAncestorCallback)(void*, uint16_t);
typedef float (*RootTransformCallback)(void*, float, float, bool);
#endif

class Artboard : public ArtboardBase,
                 public CoreContext,
                 public Virtualizable,
                 public ResettingComponent,
                 public DataBindContainer
{
    friend class File;
    friend class ArtboardImporter;
    friend class Component;

private:
    std::vector<Core*> m_Objects;
    std::vector<Core*> m_invalidObjects;
    std::vector<LinearAnimation*> m_Animations;
    std::vector<StateMachine*> m_StateMachines;
    std::vector<Component*> m_DependencyOrder;
    std::vector<Drawable*> m_Drawables;
    std::vector<DrawTarget*> m_DrawTargets;
    std::vector<NestedArtboard*> m_NestedArtboards;
    std::vector<ArtboardComponentList*> m_ComponentLists;
    std::vector<ArtboardHost*> m_ArtboardHosts;
    std::vector<Joystick*> m_Joysticks;
    std::vector<ResettingComponent*> m_Resettables;
    std::vector<ScriptedObject*> m_ScriptedObjects;
    DataContext* m_DataContext = nullptr;
    bool m_ownsDataContext = false;
    bool m_JoysticksApplyBeforeUpdate = true;

    unsigned int m_DirtDepth = 0;
    Factory* m_Factory = nullptr;
    Drawable* m_FirstDrawable = nullptr;
    bool m_IsInstance = false;
    bool m_FrameOrigin = true;
    std::unordered_set<LayoutComponent*> m_dirtyLayout;
    bool m_isCleaningDirtyLayouts = false;
    float m_originalWidth = 0;
    float m_originalHeight = 0;
    bool m_updatesOwnLayout = true;
    bool m_hostTransformMarkedDirty = false;
    Artboard* parentArtboard() const;
    ArtboardHost* m_host = nullptr;
    bool sharesLayoutWithHost() const;
    void cloneObjectDataBinds(const Core* object,
                              Core* clone,
                              Artboard* artboard) const;

    // Variable that tracks whenever the draw order changes. It is used by the
    // state machine controllers to sort their hittable components when they are
    // out of sync
    uint8_t m_drawOrderChangeCounter = 0;
#ifdef WITH_RIVE_TOOLS
    uint16_t m_artboardId = 0;
#endif
    const Artboard* m_artboardSource = nullptr;

#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    rcp<AudioEngine> m_audioEngine;
#endif

    void sortDependencies();
    void sortDrawOrder();
    void updateRenderPath() override;
    void update(ComponentDirt value) override;

public:
    void updateDataBinds(bool applyTargetToSource = true) override;
    void host(ArtboardHost* artboardHost);
    ArtboardHost* host() const;

    // Implemented for ShapePaintContainer.
    const Mat2D& shapeWorldTransform() const override
    {
        return worldTransform();
    }
    Component* virtualizableComponent() override { return this; }
    bool updatesOwnLayout() { return m_updatesOwnLayout; }
    StatusCode onAddedClean(CoreContext* context) override;
    void addDirtyDataBind(DataBind*) override;

private:
#ifdef TESTING
public:
    Artboard(Factory* factory) : m_Factory(factory) { m_Clip = true; }
#endif
    void addObject(Core* object);
    void addAnimation(LinearAnimation* object);
    void addStateMachine(StateMachine* object);

public:
    Artboard();
    ~Artboard() override;
    bool validateObjects();
    StatusCode initialize();

    Core* resolve(uint32_t id) const override;
#ifdef WITH_RIVE_TOOLS
    void artboardId(uint16_t id) { m_artboardId = id; }
    uint16_t artboardId() const { return m_artboardId; }
#endif

    void artboardSource(const Artboard* artboard)
    {
        m_artboardSource = artboard;
    }
    const Artboard* artboardSource() const
    {
        return isInstance() ? m_artboardSource : this;
    }
    bool isAncestor(const Artboard* artboard);

    /// Find the id of a component in the artboard the object in the artboard.
    /// The artboard itself has id 0 so we use that as a flag for not found.
    uint32_t idOf(Core* object) const;

    Factory* factory() const { return m_Factory; }

    // EXPERIMENTAL -- for internal testing only for now.
    // DO NOT RELY ON THIS as it may change/disappear in the future.
    Core* hitTest(HitInfo*, const Mat2D&) override;

    bool hitTestPoint(const Vec2D& position,
                      bool skipOnUnclipped,
                      bool isPrimaryHit) override;

    Vec2D rootTransform(const Vec2D&);

    void onComponentDirty(Component* component);

    /// Update components that depend on each other in DAG order.
    bool updateComponents();

    // Update layouts and components. Returns true if it updated something.
    bool updatePass(bool isRoot);

    void onDirty(ComponentDirt dirt) override;

    // Artboards don't update their world transforms in the same way
    // as other TransformComponents so we override this.
    // This is because LayoutComponent extends Drawable, but
    // Artboard is a special type of LayoutComponent
    void updateWorldTransform() override {}

    void markLayoutDirty(LayoutComponent* layoutComponent);
    void markHostTransformDirty();
    void cleanLayout(LayoutComponent* layoutComponent);

    LayoutData* takeLayoutData();
    bool syncStyleChanges() override;
    void syncStyleChangesWithUpdate(bool forceUpdate = false);
    void calculateLayout();
    bool canHaveOverrides() override { return true; }

    bool advance(float elapsedSeconds,
                 AdvanceFlags flags = AdvanceFlags::AdvanceNested |
                                      AdvanceFlags::Animate |
                                      AdvanceFlags::NewFrame);
    bool advanceInternal(float elapsedSeconds,
                         AdvanceFlags flags = AdvanceFlags::AdvanceNested |
                                              AdvanceFlags::Animate |
                                              AdvanceFlags::NewFrame);
    void reset() override;
    uint8_t drawOrderChangeCounter() { return m_drawOrderChangeCounter; }
    Drawable* firstDrawable() { return m_FirstDrawable; };
    void addScriptedObject(ScriptedObject* object);

    enum class DrawOption
    {
        kNormal,
        kHideBG,
        kHideFG,
    };
    void draw(Renderer* renderer, DrawOption option);
    void draw(Renderer* renderer) override;
    void addToRenderPath(RenderPath* path, const Mat2D& transform);
    void addToRawPath(RawPath& path, const Mat2D* transform);

#ifdef TESTING
    ShapePaintPath* clipPath() { return &m_worldPath; }
    ShapePaintPath* backgroundPath() { return &m_localPath; }
#endif

    const std::vector<Core*>& objects() const { return m_Objects; }
    template <typename T> TypedChildren<T> objects()
    {
        return TypedChildren<T>(
            Span<Core*>(m_Objects.data(), m_Objects.size()));
    }

    const std::vector<NestedArtboard*> nestedArtboards() const
    {
        return m_NestedArtboards;
    }
    const std::vector<ArtboardComponentList*> artboardComponentLists() const
    {
        return m_ComponentLists;
    }
    DataContext* dataContext() { return m_DataContext; }
    NestedArtboard* nestedArtboard(const std::string& name) const;
    NestedArtboard* nestedArtboardAtPath(const std::string& path) const;

    float originalWidth() const { return m_originalWidth; }
    float originalHeight() const { return m_originalHeight; }
    float layoutWidth() const;
    float layoutHeight() const;
    float layoutX() const;
    float layoutY() const;
    AABB bounds() const;
    AABB worldBounds() const override;
    Vec2D origin() const;
    void xChanged() override;
    void yChanged() override;

    // Can we hide these from the public? (they use playable)
    bool isTranslucent() const;
    bool isTranslucent(const LinearAnimation*) const;
    bool isTranslucent(const LinearAnimationInstance*) const;
    void dataContext(DataContext* dataContext);
    void internalDataContext(DataContext* dataContext);
    void clearDataContext();
    void unbind();
    void rebind() override;
    void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                               DataContext* parent);
    void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance);

    bool hasAudio() const;

    template <typename T = Component> T* find(const std::string& name)
    {
        for (auto object : m_Objects)
        {
            if (object != nullptr && object->is<T>() &&
                object->as<T>()->name() == name)
            {
                return static_cast<T*>(object);
            }
        }
        return nullptr;
    }

    template <typename T = Component> size_t count()
    {
        size_t count = 0;
        for (auto object : m_Objects)
        {
            if (object != nullptr && object->is<T>())
            {
                count++;
            }
        }
        return count;
    }

    template <typename T = Component> T* objectAt(size_t index)
    {
        size_t count = 0;
        for (auto object : m_Objects)
        {
            if (object != nullptr && object->is<T>())
            {
                if (count++ == index)
                {
                    return static_cast<T*>(object);
                }
            }
        }
        return nullptr;
    }

    int objectIndex(Core* component) const
    {
        int count = 0;
        for (auto object : m_Objects)
        {
            if (object == component)
            {
                return count;
            }
            count++;
        }
        return -1;
    }

    template <typename T = Component> std::vector<T*> find()
    {
        std::vector<T*> results;
        for (auto object : m_Objects)
        {
            if (object != nullptr && object->is<T>())
            {
                results.push_back(static_cast<T*>(object));
            }
        }
        return results;
    }

    size_t animationCount() const { return m_Animations.size(); }
    std::string animationNameAt(size_t index) const;

    size_t stateMachineCount() const { return m_StateMachines.size(); }
    std::string stateMachineNameAt(size_t index) const;

    LinearAnimation* firstAnimation() const { return animation(0); }
    LinearAnimation* animation(const std::string& name) const;
    LinearAnimation* animation(size_t index) const;

    StateMachine* firstStateMachine() const { return stateMachine(0); }
    StateMachine* stateMachine(const std::string& name) const;
    StateMachine* stateMachine(size_t index) const;

    /// When provided, the designer has specified that this artboard should
    /// always autoplay this StateMachine. Returns -1 if it was not
    // provided.
    int defaultStateMachineIndex() const;

    /// Make an instance of this artboard.
    template <typename T = ArtboardInstance> std::unique_ptr<T> instance() const
    {
        std::unique_ptr<T> artboardClone(new T);
        artboardClone->copy(*this);

        artboardClone->m_Factory = m_Factory;
        artboardClone->m_FrameOrigin = m_FrameOrigin;
        artboardClone->m_DataContext = m_DataContext;
        artboardClone->m_IsInstance = true;
        artboardClone->m_originalWidth = m_originalWidth;
        artboardClone->m_originalHeight = m_originalHeight;
#ifdef WITH_RIVE_TOOLS
        artboardClone->m_artboardId = m_artboardId;
#endif
        artboardClone->m_artboardSource =
            isInstance() ? m_artboardSource : this;
        cloneObjectDataBinds(this, artboardClone.get(), artboardClone.get());

        std::vector<Core*>& cloneObjects = artboardClone->m_Objects;
        cloneObjects.push_back(artboardClone.get());

        if (!m_Objects.empty())
        {
            // Skip first object (artboard).
            auto itr = m_Objects.begin();
            while (++itr != m_Objects.end())
            {
                auto object = *itr;
                cloneObjects.push_back(object == nullptr ? nullptr
                                                         : object->clone());
                // For each object, clone its data bind objects and target their
                // clones
                cloneObjectDataBinds(object,
                                     cloneObjects.back(),
                                     artboardClone.get());
            }
        }

        for (auto animation : m_Animations)
        {
            artboardClone->m_Animations.push_back(animation);
        }
        for (auto stateMachine : m_StateMachines)
        {
            artboardClone->m_StateMachines.push_back(stateMachine);
        }

        if (artboardClone->initialize() != StatusCode::Ok)
        {
            artboardClone = nullptr;
        }

        assert(artboardClone->isInstance());
        return artboardClone;
    }

    /// Returns true if the artboard is an instance of another
    bool isInstance() const { return m_IsInstance; }

    /// Returns true when the artboard will shift the origin from the top
    /// left to the relative width/height of the artboard itself. This is
    /// what the editor does visually when you change the origin value to
    /// give context as to where the origin lies within the framed bounds.
    bool frameOrigin() const { return m_FrameOrigin; }
    /// When composing multiple artboards together in a common world-space,
    /// it may be desireable to have them share the same space regardless of
    /// origin offset from the bounding artboard. Set frameOrigin to false
    /// to move the bounds relative to the origin instead of the origin
    /// relative to the bounds.
    void frameOrigin(bool value);

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        bool result = ArtboardBase::deserialize(propertyKey, reader);
        switch (propertyKey)
        {
            case widthPropertyKey:
                m_originalWidth = width();
                break;
            case heightPropertyKey:
                m_originalHeight = height();
                break;
            default:
                break;
        }
        return result;
    }

    StatusCode import(ImportStack& importStack) override;

    float volume() const;
    void volume(float value);

#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    rcp<AudioEngine> audioEngine() const;
    void audioEngine(rcp<AudioEngine> audioEngine);
#endif

#ifdef WITH_RIVE_LAYOUT
    void propagateSize() override;
#endif
private:
    float m_volume = 1.0f;
#ifdef WITH_RIVE_TOOLS
    ArtboardCallback m_layoutChangedCallback = nullptr;
    ArtboardCallback m_layoutDirtyCallback = nullptr;
    ArtboardCallback m_transformDirtyCallback = nullptr;
    TestBoundsCallback m_testBoundsCallback = nullptr;
    IsAncestorCallback m_isAncestorCallback = nullptr;
    RootTransformCallback m_rootTransformCallback = nullptr;

public:
    void* callbackUserData;
    void onLayoutChanged(ArtboardCallback callback)
    {
        m_layoutChangedCallback = callback;
    }
    void onLayoutDirty(ArtboardCallback callback)
    {
        m_layoutDirtyCallback = callback;
        addDirt(ComponentDirt::Components);
    }
    void onTransformDirty(ArtboardCallback callback)
    {
        m_transformDirtyCallback = callback;
        addDirt(ComponentDirt::Components);
    }
    void onTestBounds(TestBoundsCallback callback)
    {
        m_testBoundsCallback = callback;
    }
    void onIsAncestor(IsAncestorCallback callback)
    {
        m_isAncestorCallback = callback;
    }
    void onRootTransform(RootTransformCallback callback)
    {
        m_rootTransformCallback = callback;
    }
#endif
};

class ArtboardInstance : public Artboard
{
public:
    ArtboardInstance();
    ~ArtboardInstance() override;

    std::unique_ptr<LinearAnimationInstance> animationAt(size_t index);
    std::unique_ptr<LinearAnimationInstance> animationNamed(
        const std::string& name);

    std::unique_ptr<StateMachineInstance> stateMachineAt(size_t index);
    std::unique_ptr<StateMachineInstance> stateMachineNamed(
        const std::string& name);

    /// When provided, the designer has specified that this artboard should
    /// always autoplay this StateMachine instance. If it was not specified,
    /// this returns nullptr.
    std::unique_ptr<StateMachineInstance> defaultStateMachine();

    // This attemps to always return *something*, in this search order:
    // 1. default statemachine instance
    // 2. first statemachine instance
    // 3. first animation instance
    // 4. nullptr
    std::unique_ptr<Scene> defaultScene();

    SMIInput* input(const std::string& name, const std::string& path);
    template <typename InstType>
    InstType* getNamedInput(const std::string& name, const std::string& path);
    SMIBool* getBool(const std::string& name, const std::string& path);
    SMINumber* getNumber(const std::string& name, const std::string& path);
    SMITrigger* getTrigger(const std::string& name, const std::string& path);
    TextValueRun* getTextRun(const std::string& name, const std::string& path);
};
} // namespace rive

#endif
