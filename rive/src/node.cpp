#include "node.hpp"

using namespace rive;

void Node::onAddedClean(CoreContext* context) {}
void Node::buildDependencies()
{
	if (parent() != nullptr)
	{
		parent()->addDependent(this);
	}
}

void Node::markTransformDirty()
{
	if (!addDirt(ComponentDirt::Transform))
	{
		return;
	}
	markWorldTransformDirty();
}

void Node::markWorldTransformDirty() { addDirt(ComponentDirt::WorldTransform, true); }

void Node::updateTransform()
{
	if (rotation() != 0)
	{
		Mat2D::fromRotation(m_Transform, rotation());
	}
	else
	{
		Mat2D::identity(m_Transform);
	}
	m_Transform[4] = x();
	m_Transform[5] = y();
	Mat2D::scaleByValues(m_Transform, scaleX(), scaleY());
}

void Node::updateWorldTransform() {}

void Node::update(ComponentDirt value)
{
	if (hasDirt(value, ComponentDirt::Transform))
	{
		updateTransform();
	}
	if (hasDirt(value, ComponentDirt::WorldTransform))
	{
		updateWorldTransform();
	}
}

void Node::xChanged() { markTransformDirty(); }
void Node::yChanged() { markTransformDirty(); }
void Node::rotationChanged() { markTransformDirty(); }
void Node::scaleXChanged() { markTransformDirty(); }
void Node::scaleYChanged() { markTransformDirty(); }
void Node::opacityChanged() { markTransformDirty(); }