#include "rive/nested_artboard.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/file.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/nested_animation.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/data_bind/data_bind_path.hpp"
#include "rive/clip_result.hpp"
#include <limits>
#include <cassert>

using namespace rive;

NestedArtboard::NestedArtboard() {}
NestedArtboard::~NestedArtboard() {}

Core* NestedArtboard::clone() const
{
    NestedArtboard* nestedArtboard =
        static_cast<NestedArtboard*>(NestedArtboardBase::clone());
    nestedArtboard->file(file());
    if (m_Artboard == nullptr)
    {
        return nestedArtboard;
    }
    auto ni = m_Artboard->instance();
    nestedArtboard->nest(ni.release());
    return nestedArtboard;
}

void NestedArtboard::nest(Artboard* artboard)
{
    assert(artboard != nullptr);

    m_Artboard = artboard;
    if (!m_Artboard->isInstance())
    {
        // We're just marking the source artboard so we can later instance from
        // it. No need to advance it or change any of its properties.
        // E.g. at import time, we return here.
        return;
    }
    m_Artboard->frameOrigin(false);
    m_Artboard->opacity(renderOpacity());
    m_Artboard->volume(artboard->volume());
    m_Instance = nullptr;
    if (artboard->isInstance())
    {
        m_Instance.reset(
            static_cast<ArtboardInstance*>(artboard)); // take ownership
    }
    // This allows for swapping after initial load (after onAddedClean has
    // already been called).
    m_Artboard->host(this);
}

Artboard* NestedArtboard::findArtboard(
    ViewModelInstanceArtboard* viewModelInstanceArtboard)
{
    if (viewModelInstanceArtboard == nullptr)
    {
        return nullptr;
    }
    if (viewModelInstanceArtboard->asset() != nullptr)
    {
        if (!parentArtboard()->isAncestor(
                viewModelInstanceArtboard->asset()->artboard()))
        {
            return viewModelInstanceArtboard->asset()->artboard();
        }
        return nullptr;
    }
    else if (m_file != nullptr)
    {
        auto asset =
            m_file->artboard(viewModelInstanceArtboard->propertyValue());
        if (asset != nullptr)
        {
            auto artboardAsset = asset->as<Artboard>();
            if (!parentArtboard()->isAncestor(artboardAsset))
            {
                return artboardAsset;
            }
        }
    }
    return nullptr;
}

void NestedArtboard::clearNestedAnimations()
{
    for (auto& animation : m_NestedAnimations)
    {
        // Release the nested animation dependencies. The file will take care of
        // destroying the nested animation itself.
        animation->releaseDependencies();
    }
    m_NestedAnimations.clear();
}

void NestedArtboard::updateArtboard(
    ViewModelInstanceArtboard* viewModelInstanceArtboard)
{
    clearDataContext();
    clearNestedAnimations();
    m_boundNestedStateMachine = nullptr;
    // If asset == nullptr and propertyValue == -1, it means that the user
    // explicitly set the asset to null, so only in that case we clear the
    // artboard
    if (viewModelInstanceArtboard != nullptr &&
        viewModelInstanceArtboard->asset() == nullptr &&
        viewModelInstanceArtboard->propertyValue() == -1)
    {
        if (m_Artboard)
        {
            m_Artboard->host(nullptr);
            m_Artboard = nullptr;
        }
        m_Instance = nullptr;
        return;
    }

    Artboard* artboard = findArtboard(viewModelInstanceArtboard);
    if (artboard != nullptr)
    {
        auto artboardInstance = artboard->instance();
        if (artboard->stateMachineCount() > 0)
        {

            auto nestedStateMachine = new NestedStateMachine();
            nestedStateMachine->animationId(0);
            nestedStateMachine->initializeAnimation(artboardInstance.get());
            addNestedAnimation(nestedStateMachine);

            m_boundNestedStateMachine.reset(static_cast<NestedStateMachine*>(
                nestedStateMachine)); // take ownership
        }
        nest(artboardInstance.release());
        if (m_dataContext != nullptr && m_viewModelInstance == nullptr)
        {
            internalDataContext(m_dataContext);
        }
        else if (m_viewModelInstance != nullptr)
        {
            bindViewModelInstance(m_viewModelInstance, m_dataContext);
        }
        // TODO: @hernan review what dirt to add
        addDirt(ComponentDirt::Filthy);
    }
}

static Mat2D makeTranslate(const Artboard* artboard)
{
    return Mat2D::fromTranslate(-artboard->originX() * artboard->width(),
                                -artboard->originY() * artboard->height());
}

void NestedArtboard::draw(Renderer* renderer)
{
    if (m_needsSaveOperation)
    {
        renderer->save();
    }
    renderer->transform(worldTransform());
    m_Artboard->draw(renderer);
    if (m_needsSaveOperation)
    {
        renderer->restore();
    }
}

bool NestedArtboard::willDraw()
{
    return Super::willDraw() && m_Artboard != nullptr;
}

Core* NestedArtboard::hitTest(HitInfo* hinfo, const Mat2D& xform)
{
    if (m_Artboard == nullptr)
    {
        return nullptr;
    }
    hinfo->mounts.push_back(this);
    auto mx = xform * worldTransform() * makeTranslate(m_Artboard);
    if (auto c = m_Artboard->hitTest(hinfo, mx))
    {
        return c;
    }
    hinfo->mounts.pop_back();
    return nullptr;
}

bool NestedArtboard::hitTestHost(const Vec2D& position,
                                 bool skipOnUnclipped,
                                 ArtboardInstance* artboard)
{
    return parent()->hitTestPoint(worldTransform() * position,
                                  skipOnUnclipped,
                                  false);
}

Vec2D NestedArtboard::hostTransformPoint(const Vec2D& vec,
                                         ArtboardInstance* artboardInstance)
{
    auto localVec = Vec2D::transformMat2D(vec, worldTransform());
    auto ab = artboard();
    return ab ? ab->rootTransform(localVec) : localVec;
}

StatusCode NestedArtboard::import(ImportStack& importStack)
{
    importDataBindPath(importStack);
    auto backboardImporter =
        importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    backboardImporter->addNestedArtboard(this);

    return Super::import(importStack);
}

void NestedArtboard::addNestedAnimation(NestedAnimation* nestedAnimation)
{
    m_NestedAnimations.push_back(nestedAnimation);
}

StatusCode NestedArtboard::onAddedClean(CoreContext* context)
{
    // N.B. The nested instance will be null here for the source artboards.
    // Instances will have a nestedInstance available. This is a good thing as
    // it ensures that we only instance animations in artboard instances. It
    // does require that we always use an artboard instance (not just the source
    // artboard) when working with nested artboards, but in general this is good
    // practice for any loaded Rive file.
    assert(m_Artboard == nullptr || m_Artboard == m_Instance.get());

    if (m_Instance)
    {
        for (auto animation : m_NestedAnimations)
        {
            animation->initializeAnimation(m_Instance.get());
        }
        m_Artboard->host(this);
    }
    return Super::onAddedClean(context);
}

void NestedArtboard::update(ComponentDirt value)
{
    Super::update(value);
    if (m_Artboard == nullptr)
    {
        return;
    }
    if (hasDirt(value, ComponentDirt::RenderOpacity))
    {
        m_Artboard->opacity(renderOpacity());
    }
    if (hasDirt(value, ComponentDirt::Components))
    {
        // We intentionally discard whether or not this updated because by the
        // end of the pass all the dirt is removed and only another advance of
        // animations/statemachines can re-add it.
        m_Artboard->updatePass(false);
    }
}

bool NestedArtboard::hasNestedStateMachines() const
{
    for (auto animation : m_NestedAnimations)
    {
        if (animation->is<NestedStateMachine>())
        {
            return true;
        }
    }
    return false;
}

Span<NestedAnimation*> NestedArtboard::nestedAnimations()
{
    return m_NestedAnimations;
}

NestedArtboard* NestedArtboard::nestedArtboard(std::string name) const
{
    if (m_Instance != nullptr)
    {
        return m_Instance->nestedArtboard(name);
    }
    return nullptr;
}

NestedStateMachine* NestedArtboard::stateMachine(std::string name) const
{
    for (auto animation : m_NestedAnimations)
    {
        if (animation->is<NestedStateMachine>() && animation->name() == name)
        {
            return animation->as<NestedStateMachine>();
        }
    }
    return nullptr;
}

NestedInput* NestedArtboard::input(std::string name) const
{
    return input(name, "");
}

NestedInput* NestedArtboard::input(std::string name,
                                   std::string stateMachineName) const
{
    if (!stateMachineName.empty())
    {
        auto nestedSM = stateMachine(stateMachineName);
        if (nestedSM != nullptr)
        {
            return nestedSM->input(name);
        }
    }
    else
    {
        for (auto animation : m_NestedAnimations)
        {
            if (animation->is<NestedStateMachine>())
            {
                auto input = animation->as<NestedStateMachine>()->input(name);
                if (input != nullptr)
                {
                    return input;
                }
            }
        }
    }
    return nullptr;
}

bool NestedArtboard::worldToLocal(Vec2D world, Vec2D* local)
{
    assert(local != nullptr);
    if (m_Artboard == nullptr)
    {
        return false;
    }
    Mat2D toMountedArtboard;
    if (!worldTransform().invert(&toMountedArtboard))
    {
        return false;
    }

    *local = toMountedArtboard * world;

    return true;
}

Vec2D NestedArtboard::measureLayout(float width,
                                    LayoutMeasureMode widthMode,
                                    float height,
                                    LayoutMeasureMode heightMode)
{
    return Vec2D(std::min(widthMode == LayoutMeasureMode::undefined
                              ? std::numeric_limits<float>::max()
                              : width,
                          m_Instance ? m_Instance->width() : 0.0f),
                 std::min(heightMode == LayoutMeasureMode::undefined
                              ? std::numeric_limits<float>::max()
                              : height,
                          m_Instance ? m_Instance->height() : 0.0f));
}

void NestedArtboard::controlSize(Vec2D size,
                                 LayoutScaleType widthScaleType,
                                 LayoutScaleType heightScaleType,
                                 LayoutDirection direction)
{}

void NestedArtboard::decodeDataBindPathIds(Span<const uint8_t> value)
{
    decodeDataBindPath(value);
}

void NestedArtboard::copyDataBindPathIds(const NestedArtboardBase& object)
{
    copyDataBindPath(object.as<NestedArtboard>()->dataBindPath());
}

void NestedArtboard::internalDataContext(DataContext* value)
{
    m_dataContext = value;
    m_viewModelInstance = nullptr;
    if (artboardInstance() != nullptr)
    {
        artboardInstance()->internalDataContext(value);
        for (auto& animation : m_NestedAnimations)
        {
            if (animation->is<NestedStateMachine>())
            {
                animation->as<NestedStateMachine>()->dataContext(value);
            }
        }
    }
}

void NestedArtboard::clearDataContext()
{
    if (artboardInstance() != nullptr)
    {
        artboardInstance()->clearDataContext();
        for (auto& animation : m_NestedAnimations)
        {
            if (animation->is<NestedStateMachine>())
            {
                animation->as<NestedStateMachine>()->clearDataContext();
            }
        }
    }
}

void NestedArtboard::unbind()
{
    if (artboardInstance() != nullptr)
    {
        artboardInstance()->unbind();
    }
}

void NestedArtboard::updateDataBinds()
{
    if (artboardInstance() != nullptr && !isPaused())
    {
        artboardInstance()->updateDataBinds();
    }
}

void NestedArtboard::bindViewModelInstance(
    rcp<ViewModelInstance> viewModelInstance,
    DataContext* parent)
{
    m_dataContext = parent;
    m_viewModelInstance = viewModelInstance;
    if (artboardInstance() != nullptr)
    {
        artboardInstance()->bindViewModelInstance(viewModelInstance, parent);
        for (auto& animation : m_NestedAnimations)
        {
            if (animation->is<NestedStateMachine>())
            {
                animation->as<NestedStateMachine>()->dataContext(
                    artboardInstance()->dataContext());
            }
        }
    }
}

float NestedArtboard::calculateLocalElapsedSeconds(float elapsedSeconds)
{
    auto localElapsedSeconds = elapsedSeconds * (speed() >= 0 ? speed() : 1);
    if (quantize() >= 0)
    {
        m_cumulatedSeconds += localElapsedSeconds;
        auto quantizedSeconds = 1 / quantize();
        if (m_cumulatedSeconds > quantizedSeconds)
        {
            localElapsedSeconds =
                std::floor(m_cumulatedSeconds / quantizedSeconds) *
                quantizedSeconds;
            m_cumulatedSeconds -= localElapsedSeconds;
        }
        else
        {
            localElapsedSeconds = 0;
        }
    }
    return localElapsedSeconds;
}

bool NestedArtboard::advanceComponent(float elapsedSeconds, AdvanceFlags flags)
{
    if (m_Artboard == nullptr || isCollapsed() || isPaused())
    {
        return false;
    }
    bool keepGoing = false;
    bool advanceNested =
        (flags & AdvanceFlags::AdvanceNested) == AdvanceFlags::AdvanceNested;
    auto localElapsedSeconds = calculateLocalElapsedSeconds(elapsedSeconds);
    bool newFrame = (flags & AdvanceFlags::NewFrame) == AdvanceFlags::NewFrame;
    // If the elapsed time is 0 because of quantization, we still want to
    // continue advancing until the cumulated time is flushed
    if (localElapsedSeconds == 0 && quantize() >= 0 && newFrame)
    {
        return true;
    }
    if (advanceNested)
    {
        for (auto animation : m_NestedAnimations)
        {
            // If it is not a new frame, we only advance state machines. And we
            // first validate whether their state has changed. Then and only
            // then we advance the state machine. This avoids triggering dirt
            // from advances that make intermediate value changes but finally
            // settle in the same value
            if (!newFrame)
            {
                if (animation->is<NestedStateMachine>())
                {
                    if (animation->as<NestedStateMachine>()->tryChangeState())
                    {
                        if (animation->advance(localElapsedSeconds, newFrame))
                        {
                            keepGoing = true;
                        }
                    }
                }
            }
            else
            {

                if (animation->advance(localElapsedSeconds, newFrame))
                {
                    keepGoing = true;
                }
            }
        }
    }

    auto advancingFlags = flags & ~AdvanceFlags::IsRoot;
    if (m_Artboard->advanceInternal(localElapsedSeconds, advancingFlags))
    {
        keepGoing = true;
    }
    if (m_Artboard->hasDirt(ComponentDirt::Components))
    {
        // The animation(s) caused the artboard to need an update.
        addDirt(ComponentDirt::Components);
    }

    return keepGoing;
}

void NestedArtboard::reset()
{
    if (m_Artboard)
    {
        m_Artboard->reset();
    }
}

void NestedArtboard::file(File* value) { m_file = value; }

File* NestedArtboard::file() const { return m_file; }
