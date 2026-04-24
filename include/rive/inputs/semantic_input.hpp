#ifndef _RIVE_SEMANTIC_INPUT_HPP_
#define _RIVE_SEMANTIC_INPUT_HPP_
#include "rive/generated/inputs/semantic_input_base.hpp"

namespace rive
{
class SemanticInput : public SemanticInputBase
{
public:
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif
