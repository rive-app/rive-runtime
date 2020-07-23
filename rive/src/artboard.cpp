#include "artboard.hpp"
#include "animation/animation.hpp"
#include "dependency_sorter.hpp"

using namespace rive;

Artboard::~Artboard()
{
	for (auto object : m_Objects)
	{
		// First object is artboard
		if (object == this)
		{
			continue;
		}
		delete object;
	}
	for (auto object : m_Animations)
	{
		delete object;
	}
}

void Artboard::onAddedDirty(CoreContext* context)
{
	// Intentionally empty and intentionally doesn't call
	// Super::onAddedDirty(context); to avoid parenting to self.
}

void Artboard::initialize()
{
	// onAddedDirty guarantees that all objects are now available so they can be
	// looked up by index/id. This is where nodes find their parents, but they
	// can't assume that their parent's parent will have resolved yet.
	for (auto object : m_Objects)
	{
		object->onAddedDirty(this);
	}

	for (auto object : m_Animations)
	{
		object->onAddedDirty(this);
	}

	// onAddedClean is called when all individually referenced components have
	// been found and so components can look at other components' references and
	// assume that they have resolved too. This is where the whole hierarchy is
	// linked up and we can traverse it to find other references (my parent's
	// parent should be type X can be checked now).
	for (auto object : m_Objects)
	{
		object->onAddedClean(this);
	}
	for (auto object : m_Animations)
	{
		object->onAddedClean(this);
	}
	// Multi-level references have been built up, now we can actually mark
	// what's dependent on what.
	for (auto object : m_Objects)
	{
		if (object->is<Component>())
		{
			object->as<Component>()->buildDependencies();
		}
	}

	sortDependencies();
}

void Artboard::sortDependencies()
{
	DependencySorter sorter;
	sorter.sort(this, m_DependencyOrder);
	unsigned int graphOrder = 0;
	for (auto component : m_DependencyOrder)
	{
		component->m_GraphOrder = graphOrder++;
	}
	m_Dirt |= ComponentDirt::Components;
}

void Artboard::addObject(Core* object) { m_Objects.push_back(object); }

void Artboard::addAnimation(Animation* object)
{
	m_Animations.push_back(object);
}

Core* Artboard::resolve(int id) const
{
	if (id < 0 || id >= m_Objects.size())
	{
		return nullptr;
	}
	return m_Objects[id];
}

void Artboard::onComponentDirty(Component* component)
{
	if (hasDirt(ComponentDirt::Components))
	{
		// TODO: we need to tell the system that it needs to advance/draw
		// another frame ASAP.
		//   context?.markNeedsAdvance();
		m_Dirt |= ComponentDirt::Components;
	}

	/// If the order of the component is less than the current dirt depth,
	/// update the dirt depth so that the update loop can break out early and
	/// re-run (something up the tree is dirty).
	if (component->graphOrder() < m_DirtDepth)
	{
		m_DirtDepth = component->graphOrder();
	}
}

bool Artboard::updateComponents()
{
	if (hasDirt(ComponentDirt::Components))
	{
		const int maxSteps = 100;
		int step = 0;
		int count = m_DependencyOrder.size();
		while (hasDirt(ComponentDirt::Components) && step < maxSteps)
		{
			m_Dirt &= ~ComponentDirt::Components;

			// Track dirt depth here so that if something else marks
			// dirty, we restart.
			for (int i = 0; i < count; i++)
			{
				auto component = m_DependencyOrder[i];
				m_DirtDepth = i;
				auto d = component->m_Dirt;
				if (d == ComponentDirt::None)
				{
					continue;
				}
				component->m_Dirt = ComponentDirt::None;
				component->update(d);

				// If the update changed the dirt depth by adding dirt to
				// something before us (in the DAG), early out and re-run the
				// update.
				if (m_DirtDepth < i)
				{
					// We put this in here just to know if we need to keep this
					// around...
					assert(false);
					break;
				}
			}
			step++;
		}
		return true;
	}
	return false;
}

bool Artboard::advance(double elapsedSeconds) { return updateComponents(); }