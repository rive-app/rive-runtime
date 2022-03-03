#include "rive/shapes/points_path.hpp"
#include "rive/shapes/vertex.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/bones/skin.hpp"
#include "rive/span.hpp"

using namespace rive;

Mat2D identity;
void PointsPath::buildDependencies() {
    Super::buildDependencies();
    if (skin() != nullptr) {
        skin()->addDependent(this);
    }
}

const Mat2D& PointsPath::pathTransform() const {
    if (skin() != nullptr) {
        return identity;
    }
    return worldTransform();
}

void PointsPath::update(ComponentDirt value) {
    if (hasDirt(value, ComponentDirt::Path) && skin() != nullptr) {
        skin()->deform(Span((Vertex**)m_Vertices.data(), m_Vertices.size()));
    }
    Super::update(value);
}

void PointsPath::markPathDirty() {
    if (skin() != nullptr) {
        skin()->addDirt(ComponentDirt::Skin);
    }
    Super::markPathDirty();
}

void PointsPath::markSkinDirty() { markPathDirty(); }