#include "node.hpp"

using namespace rive;

void Node::xChanged() { markTransformDirty(); }
void Node::yChanged() { markTransformDirty(); }
void Node::rotationChanged() { markTransformDirty(); }
void Node::scaleXChanged() { markTransformDirty(); }
void Node::scaleYChanged() { markTransformDirty(); }
void Node::opacityChanged() { markTransformDirty(); }