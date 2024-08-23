#include "rive/static_scene.hpp"
#include "rive/artboard.hpp"

using namespace rive;

StaticScene::StaticScene(ArtboardInstance* instance) : Scene(instance) {}

StaticScene::~StaticScene() {}

bool StaticScene::isTranslucent() const { return m_artboardInstance->isTranslucent(); };

std::string StaticScene::name() const { return m_artboardInstance->name(); };

Loop StaticScene::loop() const { return Loop::oneShot; };

float StaticScene::durationSeconds() const { return 0; }

bool StaticScene::advanceAndApply(float seconds)
{
    // We ignore the 'seconds' argument because it's not an animated scene
    m_artboardInstance->advance(0);
    return true;
};