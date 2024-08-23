#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "rive/component.hpp"
#include "rive/refcnt.hpp"
#include "rive/math/raw_path.hpp"

namespace rive
{
class Shape;
class CommandPath;
class RenderPath;
class PathComposer : public Component
{

public:
    PathComposer(Shape* shape);
    Shape* shape() const { return m_shape; }
    void buildDependencies() override;
    void onDirty(ComponentDirt dirt) override;
    void update(ComponentDirt value) override;

    CommandPath* localPath() const { return m_localPath.get(); }
    CommandPath* worldPath() const { return m_worldPath.get(); }

    const RawPath& localRawPath() const { return m_localRawPath; }
    const RawPath& worldRawPath() const { return m_worldRawPath; }

    void pathCollapseChanged();

private:
    Shape* m_shape;
    RawPath m_localRawPath;
    RawPath m_worldRawPath;
    rcp<CommandPath> m_localPath;
    rcp<CommandPath> m_worldPath;
    bool m_deferredPathDirt;
};
} // namespace rive
#endif
