#include "rive/nested_artboard.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/nested_animation.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/clip_result.hpp"
#include <cassert>

using namespace rive;

NestedArtboard::NestedArtboard() {}
NestedArtboard::~NestedArtboard() {}

Core* NestedArtboard::clone() const
{
    NestedArtboard* nestedArtboard = static_cast<NestedArtboard*>(NestedArtboardBase::clone());
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
        return;
    }
    m_Artboard->frameOrigin(false);
    m_Artboard->opacity(renderOpacity());
    m_Instance = nullptr;
    if (artboard->isInstance())
    {
        m_Instance.reset(static_cast<ArtboardInstance*>(artboard)); // take ownership
    }
    m_Artboard->advance(0.0f);
}

static Mat2D makeTranslate(const Artboard* artboard)
{
    return Mat2D::fromTranslate(-artboard->originX() * artboard->width(),
                                -artboard->originY() * artboard->height());
}

void NestedArtboard::draw(Renderer* renderer)
{
    if (m_Artboard == nullptr)
    {
        return;
    }
    ClipResult clipResult = clip(renderer);
    if (clipResult == ClipResult::noClip)
    {
        // We didn't clip, so make sure to save as we'll be doing some
        // transformations.
        renderer->save();
    }
    if (clipResult != ClipResult::emptyClip)
    {
        renderer->transform(worldTransform());
        m_Artboard->draw(renderer);
    }
    renderer->restore();
}

Core* NestedArtboard::hitTest(HitInfo* hinfo, const Mat2D& xform)
{
    if (m_Artboard == nullptr)
    {
        return nullptr;
    }
    hinfo->mounts.push_back(this);
    auto mx = xform * worldTransform() * makeTranslate(m_Artboard);
    if (auto c = m_Artboard->hitTest(hinfo, &mx))
    {
        return c;
    }
    hinfo->mounts.pop_back();
    return nullptr;
}

StatusCode NestedArtboard::import(ImportStack& importStack)
{
    auto backboardImporter = importStack.latest<BackboardImporter>(Backboard::typeKey);
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
    }
    return Super::onAddedClean(context);
}

bool NestedArtboard::advance(float elapsedSeconds)
{
    if (m_Artboard == nullptr || isCollapsed())
    {
        return false;
    }
    for (auto animation : m_NestedAnimations)
    {
        animation->advance(elapsedSeconds);
    }
    return m_Artboard->advance(elapsedSeconds);
}

void NestedArtboard::update(ComponentDirt value)
{
    Super::update(value);
    if (hasDirt(value, ComponentDirt::RenderOpacity) && m_Artboard != nullptr)
    {
        m_Artboard->opacity(renderOpacity());
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

Span<NestedAnimation*> NestedArtboard::nestedAnimations() { return m_NestedAnimations; }

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