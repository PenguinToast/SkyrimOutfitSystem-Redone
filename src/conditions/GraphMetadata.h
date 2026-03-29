#pragma once

#include <string>
#include <vector>

namespace sosr::conditions {
struct GraphMetadata {
  std::vector<std::string> referencedConditionIds;
  std::vector<std::string> reverseDependencyIds;

  void Clear() {
    referencedConditionIds.clear();
    reverseDependencyIds.clear();
  }
};
} // namespace sosr::conditions
