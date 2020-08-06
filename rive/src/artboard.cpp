#include "artboard.hpp"
#include "animation/animation.hpp"
#include "dependency_sorter.hpp"
#include "drawable.hpp"
#include "node.hpp"
#include "renderer.hpp"
#include "shapes/paint/shape_paint.hpp"

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
	delete m_RenderPath;
}

void Artboard::initialize()
{
	m_RenderPath = makeRenderPath();

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
		if (object->is<Drawable>())
		{
			m_Drawables.push_back(object->as<Drawable>());
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
	m_Dirt |= ComponentDirt::Components;

	/// If the order of the component is less than the current dirt depth,
	/// update the dirt depth so that the update loop can break out early and
	/// re-run (something up the tree is dirty).
	if (component->graphOrder() < m_DirtDepth)
	{
		m_DirtDepth = component->graphOrder();
	}
}

void Artboard::onDirty(ComponentDirt dirt)
{
	m_Dirt |= ComponentDirt::Components;
}

static bool drawOrderComparer(Drawable* a, Drawable* b)
{
	return a->drawOrder() < b->drawOrder();
}

void Artboard::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::DrawOrder))
	{
		std::sort(m_Drawables.begin(), m_Drawables.end(), drawOrderComparer);
	}
	if (hasDirt(value, ComponentDirt::Path))
	{
		m_RenderPath->reset();
		m_RenderPath->addRect(0.0f, 0.0f, width(), height());
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
			// m_Dirt = m_Dirt & ~ComponentDirt::Components;

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

void Artboard::draw(Renderer* renderer)
{
	for (auto shapePaint : m_ShapePaints)
	{
		shapePaint->draw(renderer, m_RenderPath);
	}
	renderer->save();
	renderer->clipPath(m_RenderPath);

	Mat2D artboardTransform;
	artboardTransform[4] = width() * originX();
	artboardTransform[5] = height() * originY();
	renderer->transform(artboardTransform);

	for (auto drawable : m_Drawables)
	{
		drawable->draw(renderer);
	}
	renderer->restore();
}

AABB Artboard::bounds() const
{
	auto x = -originX() * width();
	auto y = -originY() * height();
	return AABB(x, y, x + width(), y + height());
}

Node* Artboard::node(const std::string& name) const
{
	for (auto object : m_Objects)
	{
		// First object is artboard
		if (object->is<Node>() && object->as<Node>()->name() == name)
		{
			return object->as<Node>();
		}
	}
	return nullptr;
}