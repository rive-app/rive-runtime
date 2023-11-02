#ifndef _RIVE_STATIC_SCENE_HPP_
#define _RIVE_STATIC_SCENE_HPP_

#include "rive/artboard.hpp"
#include "rive/scene.hpp"

namespace rive
{
class StaticScene : public Scene
{
public:
    StaticScene(ArtboardInstance*);
    ~StaticScene() override;

    bool isTranslucent() const override;

    std::string name() const override;

    Loop loop() const override;

    float durationSeconds() const override;

    bool advanceAndApply(float seconds) override;
};
} // namespace rive
#endif
