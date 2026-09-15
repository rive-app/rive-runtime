#include "rive/bones/skin.hpp"
#include "rive/bones/bone.hpp"
#include "rive/bones/skinnable.hpp"
#include "rive/bones/tendon.hpp"
#include "rive/shapes/vertex.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/constraints/constraint.hpp"
#include <algorithm>

using namespace rive;

Skin::~Skin() { delete[] m_BoneTransforms; }

StatusCode Skin::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    m_WorldTransform[0] = xx();
    m_WorldTransform[1] = xy();
    m_WorldTransform[2] = yx();
    m_WorldTransform[3] = yy();
    m_WorldTransform[4] = tx();
    m_WorldTransform[5] = ty();
    m_Skinnable = Skinnable::from(parent());
    if (m_Skinnable == nullptr)
    {
        return StatusCode::MissingObject;
    }

#ifndef WITH_RIVE_EDITOR
    // Runtime-only; editor build registers via editorParentChanged.
    m_Skinnable->skin(this);
#endif

    return StatusCode::Ok;
}

void Skin::update(ComponentDirt value)
{
    int bidx = 6;
    for (auto tendon : m_Tendons)
    {
#ifdef WITH_RIVE_EDITOR
        // Edit-time: a partially-hydrated coop batch can leave a
        // Tendon with a null `m_Bone` (its Bone hasn't been delivered
        // yet, or was deleted). Without this guard `update` reads the
        // null bone's world transform and crashes mid-frame. Emit the
        // identity transform so the skin can still render in its
        // bind pose until the bone reappears.
        if (tendon->bone() == nullptr)
        {
            const auto& inv = tendon->inverseBind();
            m_BoneTransforms[bidx++] = inv[0];
            m_BoneTransforms[bidx++] = inv[1];
            m_BoneTransforms[bidx++] = inv[2];
            m_BoneTransforms[bidx++] = inv[3];
            m_BoneTransforms[bidx++] = inv[4];
            m_BoneTransforms[bidx++] = inv[5];
            continue;
        }
#endif
        auto world = tendon->bone()->worldTransform() * tendon->inverseBind();
        m_BoneTransforms[bidx++] = world[0];
        m_BoneTransforms[bidx++] = world[1];
        m_BoneTransforms[bidx++] = world[2];
        m_BoneTransforms[bidx++] = world[3];
        m_BoneTransforms[bidx++] = world[4];
        m_BoneTransforms[bidx++] = world[5];
    }
}

void Skin::buildDependencies()
{
    // depend on bones from tendons
    for (auto tendon : m_Tendons)
    {
        auto bone = tendon->bone();
#ifdef WITH_RIVE_EDITOR
        // See `Skin::update` — coop can leave an unresolved Tendon
        // with a null bone. `editor_native/finalizeBatch` calls
        // `Tendon::resolveBone` on every tendon before us, so this
        // is only hit for tendons whose bone genuinely isn't in the
        // file yet. Skip; the dependency edge rewires when the bone
        // arrives in a later batch and we re-run finalizeBatch.
        if (bone == nullptr)
        {
            continue;
        }
#endif
        bone->addDependent(this);
        for (auto constraint : bone->peerConstraints())
        {
            constraint->parent()->addDependent(this);
        }
    }

#ifdef WITH_RIVE_EDITOR
    // Editor re-runs buildDependencies after every batch (Pass-B-prep
    // wipes the dependents list, then the typed per-class method
    // rewires it). For Skin that means a second allocation of the
    // bone-transform buffer — `assert` below would catch the runtime-
    // only invariant. Free the prior buffer so the editor case stays
    // leak-free while keeping the runtime guard intact.
    if (m_BoneTransforms != nullptr)
    {
        delete[] m_BoneTransforms;
        m_BoneTransforms = nullptr;
    }
#else
    // Make sure no-one is calling this twice.
    assert(m_BoneTransforms == nullptr);
#endif
    // We can now init the bone buffer.
    auto size = (m_Tendons.size() + 1) * 6;
    m_BoneTransforms = new float[size];
    m_BoneTransforms[0] = 1;
    m_BoneTransforms[1] = 0;
    m_BoneTransforms[2] = 0;
    m_BoneTransforms[3] = 1;
    m_BoneTransforms[4] = 0;
    m_BoneTransforms[5] = 0;
}

void Skin::deform(Span<Vertex*> vertices)
{
    for (auto vertex : vertices)
    {
        vertex->deform(m_WorldTransform, m_BoneTransforms);
    }
}
void Skin::addTendon(Tendon* tendon) { m_Tendons.push_back(tendon); }

#ifdef WITH_RIVE_EDITOR
void Skin::sortTendonsForEditor()
{
    auto byChildOrder = [](const Tendon* a, const Tendon* b) {
        return a->childOrder().compareTo(b->childOrder()) < 0;
    };
    if (std::is_sorted(m_Tendons.begin(), m_Tendons.end(), byChildOrder))
    {
        return;
    }
    std::sort(m_Tendons.begin(), m_Tendons.end(), byChildOrder);
    // Weights index m_BoneTransforms by tendon position, so a reorder has to
    // rebuild the buffer and re-deform the skinnable.
    addDirt(ComponentDirt::WorldTransform);
}
#endif

void Skin::onDirty(ComponentDirt dirt)
{
    if (m_Skinnable != nullptr)
    {
        m_Skinnable->markSkinDirty();
    }
}
