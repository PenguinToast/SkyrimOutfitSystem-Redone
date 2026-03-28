#pragma once

#include "imgui.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sosr::ui::conditions {
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

struct Definition {
  std::string id;
  std::string name;
  std::string description;
  ImVec4 color{0.55f, 0.55f, 0.55f, 1.0f};
  std::vector<Clause> clauses;
};
} // namespace sosr::ui::conditions
