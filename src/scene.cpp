#include "rive/artboard.hpp"
#include "rive/hit_result.hpp"
#include "rive/scene.hpp"
#include "rive/generated/core_registry.hpp"
using namespace rive;

Scene::Scene(ArtboardInstance* abi) : m_artboardInstance(abi)
{
    assert(m_artboardInstance->isInstance());
}

float Scene::width() const { return m_artboardInstance->width(); }

float Scene::height() const { return m_artboardInstance->height(); }

void Scene::draw(Renderer* renderer) { m_artboardInstance->draw(renderer); }

HitResult Scene::pointerDown(Vec2D) { return HitResult::none; }
HitResult Scene::pointerMove(Vec2D) { return HitResult::none; }
HitResult Scene::pointerUp(Vec2D) { return HitResult::none; }
HitResult Scene::pointerExit(Vec2D) { return HitResult::none; }

size_t Scene::inputCount() const { return 0; }
SMIInput* Scene::input(size_t index) const { return nullptr; }
SMIBool* Scene::getBool(const std::string&) const { return nullptr; }
SMINumber* Scene::getNumber(const std::string&) const { return nullptr; }
SMITrigger* Scene::getTrigger(const std::string&) const { return nullptr; }

void Scene::reportKeyedCallback(uint32_t objectId, uint32_t propertyKey, float elapsedSeconds)
{
    auto coreObject = m_artboardInstance->resolve(objectId);
    CallbackData data(this, elapsedSeconds);
    CoreRegistry::setCallback(coreObject, propertyKey, data);
}
