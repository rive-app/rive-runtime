#ifndef _RIVE_SKIN_HPP_
#define _RIVE_SKIN_HPP_
#include "rive/generated/bones/skin_base.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/span.hpp"
#include <stdio.h>
#include <vector>

namespace rive
{
class Tendon;
class Vertex;
class Skinnable;

class Skin : public SkinBase
{
    friend class Tendon;

public:
    ~Skin() override;

private:
    Mat2D m_WorldTransform;
    std::vector<Tendon*> m_Tendons;
    float* m_BoneTransforms = nullptr;
    Skinnable* m_Skinnable;

protected:
    void addTendon(Tendon* tendon);

public:
    StatusCode onAddedDirty(CoreContext* context) override;
    void buildDependencies() override;
    void deform(Span<Vertex*> vertices);
    void onDirty(ComponentDirt dirt) override;
    void update(ComponentDirt value) override;
#ifdef WITH_RIVE_EDITOR
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
    // Editor-only readback of the accumulated bone transforms array
    // (slot 0 = identity, slot N+1 = `bone[N].worldTransform *
    // tendon[N].inverseBind`, 6 floats per slot). Tangent / vertex
    // translation setters invert per-slot weights against this to
    // map post-deform cursor positions back to path-local
    // coordinates — dart's `Weight.computeDeformTransform` reads the
    // same array via `skin.boneTransforms`.
    const float* boneTransforms() const { return m_BoneTransforms; }
    // Editor-only counterpart to `Path::sortVerticesForEditor`. Coop
    // hydration delivers Tendons in server-batch arrival order, not
    // childOrder; `Tendon::onAddedClean` pushes them onto `m_Tendons`
    // in that arrival order. Vertex weights index into `m_Tendons` by
    // position, so a load-time arrival-order shuffle silently maps
    // each weight to the wrong bone. `finalizeBatch` calls this on
    // every Skin after hydration to restore the author-time order.
    void sortTendonsForEditor();
#endif

#ifdef TESTING
    std::vector<Tendon*>& tendons() { return m_Tendons; }
#endif
};
} // namespace rive

#endif