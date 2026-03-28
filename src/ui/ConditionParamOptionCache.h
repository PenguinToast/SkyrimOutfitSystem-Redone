#pragma once

#include "RE/C/CommandTable.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace sosr::ui::conditions {
class ConditionParamOptionCache {
public:
  enum class State : std::uint8_t { Unsupported, Loading, Ready };

  static ConditionParamOptionCache &Get();

  [[nodiscard]] static bool Supports(RE::SCRIPT_PARAM_TYPE a_type);
  void Continue(double a_maxMillisecondsPerTick = 16.0);
  void Reset();
  [[nodiscard]] State Ensure(RE::SCRIPT_PARAM_TYPE a_type);
  [[nodiscard]] const std::vector<std::string> *
  GetOptions(RE::SCRIPT_PARAM_TYPE a_type) const;
  [[nodiscard]] float GetProgress(RE::SCRIPT_PARAM_TYPE a_type) const;
  [[nodiscard]] std::string_view GetStatus(RE::SCRIPT_PARAM_TYPE a_type) const;

private:
  ConditionParamOptionCache() = default;
};
} // namespace sosr::ui::conditions
