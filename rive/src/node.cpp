#include "node.hpp"

using namespace rive;

void Node::onAddedClean(CoreContext* context)
{
	m_ParentNode = parent() != nullptr && parent()->is<Node>() ? parent()->as<Node>() : nullptr;
}

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

void Node::updateWorldTransform()
{
	m_RenderOpacity = opacity();
	if (m_ParentNode != nullptr)
	{
		m_RenderOpacity *= m_ParentNode->childOpacity();
		Mat2D::multiply(m_WorldTransform, m_ParentNode->m_WorldTransform, m_Transform);
	}
	else
	{
		Mat2D::copy(m_WorldTransform, m_Transform);
	}
}

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

const Mat2D& Node::transform() const { return m_Transform; }
const Mat2D& Node::worldTransform() const { return m_WorldTransform; }

void Node::xChanged() { markTransformDirty(); }
void Node::yChanged() { markTransformDirty(); }
void Node::rotationChanged() { markTransformDirty(); }
void Node::scaleXChanged() { markTransformDirty(); }
void Node::scaleYChanged() { markTransformDirty(); }
void Node::opacityChanged() { markTransformDirty(); }