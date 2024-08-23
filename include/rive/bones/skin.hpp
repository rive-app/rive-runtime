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

#ifdef TESTING
    std::vector<Tendon*>& tendons() { return m_Tendons; }
#endif
};
} // namespace rive

#endif