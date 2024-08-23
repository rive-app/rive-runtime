#include "rive/node.hpp"

using namespace rive;

void Node::xChanged() { markTransformDirty(); }
void Node::yChanged() { markTransformDirty(); }