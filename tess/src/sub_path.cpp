#include "rive/tess/sub_path.hpp"

using namespace rive;

SubPath::SubPath(RenderPath* path, const Mat2D& transform) : m_Path(path), m_Transform(transform) {}

RenderPath* SubPath::path() const { return m_Path; }
const Mat2D& SubPath::transform() const { return m_Transform; }