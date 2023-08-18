#include "rive/artboard.hpp"
#include "rive/scene.hpp"

using namespace rive;

Scene::Scene(ArtboardInstance* abi) : m_artboardInstance(abi)
{
    assert(m_artboardInstance->isInstance());
}

float Scene::width() const { return m_artboardInstance->width(); }

float Scene::height() const { return m_artboardInstance->height(); }

void Scene::draw(Renderer* renderer) { m_artboardInstance->draw(renderer); }

void Scene::pointerDown(Vec2D) {}
void Scene::pointerMove(Vec2D) {}
void Scene::pointerUp(Vec2D) {}

size_t Scene::inputCount() const { return 0; }
SMIInput* Scene::input(size_t index) const { return nullptr; }
SMIBool* Scene::getBool(const std::string&) const { return nullptr; }
SMINumber* Scene::getNumber(const std::string&) const { return nullptr; }
SMITrigger* Scene::getTrigger(const std::string&) const { return nullptr; }
