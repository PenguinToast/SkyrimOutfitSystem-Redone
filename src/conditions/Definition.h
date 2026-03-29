#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace RE {
class TESCondition;
}

namespace sosr::conditions {
inline constexpr std::string_view kDefaultConditionId = "condition-1";

enum class Comparator : std::uint8_t {
  Equal,
  NotEqual,
  Greater,
  GreaterOrEqual,
  Less,
  LessOrEqual
};

enum class Connective : std::uint8_t { And, Or };

struct Clause {
  std::string functionName;
  std::string customConditionId;
  std::array<std::string, 2> arguments{};
  Comparator comparator{Comparator::Equal};
  std::string comparand{"1"};
  Connective connectiveToNext{Connective::And};
};

struct Color {
  float x{0.55f};
  float y{0.55f};
  float z{0.55f};
  float w{1.0f};
};

struct TransientMaterializationCache {
  bool valid{false};
  std::shared_ptr<RE::TESCondition> condition;
  std::string signature;
  std::vector<std::uint32_t> refreshActorFormIDs;
  bool refreshUseNearbyFallback{false};

  void Clear() {
    valid = false;
    condition.reset();
    signature.clear();
    refreshActorFormIDs.clear();
    refreshUseNearbyFallback = false;
  }
};

struct Definition {
  std::string id;
  std::string name;
  std::string description;
  Color color{};
  std::vector<Clause> clauses;
  std::vector<std::string> referencedConditionIds;
  std::vector<std::string> reverseDependencyIds;
  TransientMaterializationCache transientCache;
};
} // namespace sosr::conditions
