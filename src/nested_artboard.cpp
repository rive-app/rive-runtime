#include "rive/nested_artboard.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/nested_animation.hpp"

using namespace rive;

NestedArtboard::~NestedArtboard() {
    if (m_NestedInstance->isInstance()) {
        delete m_NestedInstance;
    }
}
Core* NestedArtboard::clone() const {
    NestedArtboard* nestedArtboard = static_cast<NestedArtboard*>(NestedArtboardBase::clone());
    if (m_NestedInstance == nullptr) {
        return nestedArtboard;
    }
    auto ni = m_NestedInstance->instance();
    assert(ni->isInstance());
    nestedArtboard->nest(ni.release());
    return nestedArtboard;
}

void NestedArtboard::nest(Artboard* artboard) {
    assert(artboard != nullptr);
    m_NestedInstance = artboard;
    m_NestedInstance->advance(0.0f);
}

static Mat2D makeTranslate(const Artboard* artboard) {
    return Mat2D::fromTranslate(-artboard->originX() * artboard->width(),
                                -artboard->originY() * artboard->height());
}

void NestedArtboard::draw(Renderer* renderer) {
    if (m_NestedInstance == nullptr) {
        return;
    }
    if (!clip(renderer)) {
        // We didn't clip, so make sure to save as we'll be doing some
        // transformations.
        renderer->save();
    }
    renderer->transform(worldTransform() * makeTranslate(m_NestedInstance));
    m_NestedInstance->draw(renderer);
    renderer->restore();
}

Core* NestedArtboard::hitTest(HitInfo* hinfo, const Mat2D& xform) {
    if (m_NestedInstance == nullptr) {
        return nullptr;
    }
    hinfo->mounts.push_back(this);
    auto mx = xform * worldTransform() * makeTranslate(m_NestedInstance);
    if (auto c = m_NestedInstance->hitTest(hinfo, &mx)) {
        return c;
    }
    hinfo->mounts.pop_back();
    return nullptr;
}

StatusCode NestedArtboard::import(ImportStack& importStack) {
    auto backboardImporter = importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr) {
        return StatusCode::MissingObject;
    }
    backboardImporter->addNestedArtboard(this);

    return Super::import(importStack);
}

void NestedArtboard::addNestedAnimation(NestedAnimation* nestedAnimation) {
    m_NestedAnimations.push_back(nestedAnimation);
}

StatusCode NestedArtboard::onAddedClean(CoreContext* context) {
    // N.B. The nested instance will be null here for the source artboards.
    // Instances will have a nestedInstance available. This is a good thing as
    // it ensures that we only instance animations in artboard instances. It
    // does require that we always use an artboard instance (not just the source
    // artboard) when working with nested artboards, but in general this is good
    // practice for any loaded Rive file.
    if (m_NestedInstance != nullptr) {
        for (auto animation : m_NestedAnimations) {
            animation->initializeAnimation(m_NestedInstance);
        }
    }
    return Super::onAddedClean(context);
}

bool NestedArtboard::advance(float elapsedSeconds) {
    if (m_NestedInstance == nullptr) {
        return false;
    }
    for (auto animation : m_NestedAnimations) {
        animation->advance(elapsedSeconds);
    }
    return m_NestedInstance->advance(elapsedSeconds);
}

void NestedArtboard::update(ComponentDirt value) {
    Super::update(value);
    if (hasDirt(value, ComponentDirt::WorldTransform) && m_NestedInstance != nullptr) {
        m_NestedInstance->opacity(renderOpacity());
    }
}
