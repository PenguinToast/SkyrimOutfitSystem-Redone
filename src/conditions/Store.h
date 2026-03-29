#pragma once

#include "conditions/Definition.h"

#include <vector>

namespace sosr::conditions {
struct Store {
  int nextConditionId{1};
  std::vector<Definition> definitions;
};
} // namespace sosr::conditions
