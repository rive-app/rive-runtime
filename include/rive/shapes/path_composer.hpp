#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "rive/component.hpp"
#include "rive/refcnt.hpp"
namespace rive
{
class Shape;
class CommandPath;
class RenderPath;
class PathComposer : public Component
{
private:
    Shape* m_Shape;
    rcp<CommandPath> m_LocalPath;
    rcp<CommandPath> m_WorldPath;
    bool m_deferredPathDirt;

public:
    PathComposer(Shape* shape);
    Shape* shape() const { return m_Shape; }
    void buildDependencies() override;
    void onDirty(ComponentDirt dirt) override;
    void update(ComponentDirt value) override;

    CommandPath* localPath() const { return m_LocalPath.get(); }
    CommandPath* worldPath() const { return m_WorldPath.get(); }

    void pathCollapseChanged();
};
} // namespace rive
#endif
