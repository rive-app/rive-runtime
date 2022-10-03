#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "rive/component.hpp"
namespace rive
{
class Shape;
class CommandPath;
class RenderPath;
class PathComposer : public Component
{
private:
    Shape* m_Shape;
    std::unique_ptr<CommandPath> m_LocalPath;
    std::unique_ptr<CommandPath> m_WorldPath;

public:
    PathComposer(Shape* shape);
    Shape* shape() const { return m_Shape; }
    void buildDependencies() override;
    void update(ComponentDirt value) override;

    CommandPath* localPath() const { return m_LocalPath.get(); }
    CommandPath* worldPath() const { return m_WorldPath.get(); }
};
} // namespace rive
#endif
