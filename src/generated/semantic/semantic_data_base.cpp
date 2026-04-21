#include "rive/generated/semantic/semantic_data_base.hpp"
#include "rive/semantic/semantic_data.hpp"

using namespace rive;

Core* SemanticDataBase::clone() const
{
    auto cloned = new SemanticData();
    cloned->copy(*this);
    return cloned;
}
