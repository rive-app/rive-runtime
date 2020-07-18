#include "artboard.hpp"
#include "animation/animation.hpp"

using namespace rive;

Artboard::~Artboard()
{
	for (auto object : m_Objects)
	{
		delete object;
	}
	for (auto object : m_Animations)
	{
		delete object;
	}
}

void Artboard::initialize()
{
	for (auto object : m_Objects)
	{
		object->onAddedDirty(this);
	}
	for (auto object : m_Animations)
	{
		object->onAddedDirty(this);
	}
	for (auto object : m_Objects)
	{
		object->onAddedClean(this);
	}
	for (auto object : m_Animations)
	{
		object->onAddedClean(this);
	}
}

void Artboard::addObject(Core* object) { m_Objects.push_back(object); }

void Artboard::addAnimation(Animation* object) { m_Animations.push_back(object); }

Core* Artboard::resolve(int id) const
{
	if (id < 0 || id >= m_Objects.size())
	{
		return nullptr;
	}
	return m_Objects[id];
}

Core* Artboard::find(std::string name)
{
	for (auto object : m_Objects)
	{
		if (object->is<Component>() && object->as<Component>()->name() == name)
		{

			return object;
		}
	}
	return nullptr;
}