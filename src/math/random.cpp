/*
 * Copyright 2022 Rive
 */

#include "rive/math/random.hpp"

namespace rive
{

#ifdef TESTING
int RandomProvider::m_randomCallsCount = 0;
std::vector<float> RandomProvider::m_randomResults;
#endif

} // namespace rive
