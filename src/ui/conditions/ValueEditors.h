#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace sosr::ui::condition_editor {
enum class ValueEditorKind : std::uint8_t {
  Unsupported,
  Integer,
  Number,
  CachedOption
};

[[nodiscard]] ValueEditorKind
GetEditorKindForParamType(RE::SCRIPT_PARAM_TYPE a_type);
[[nodiscard]] RE::SCRIPT_PARAM_TYPE
ResolveEditorParamType(std::string_view a_functionName,
                       std::uint16_t a_paramIndex,
                       RE::SCRIPT_PARAM_TYPE a_type);
[[nodiscard]] std::string FormatNumberString(double a_value);

bool DrawNumericClauseValueEditor(const char *a_id, std::string &a_value,
                                  ValueEditorKind a_kind, float a_width);
bool DrawConditionParamEditor(const char *a_id, std::string &a_value,
                              RE::SCRIPT_PARAM_TYPE a_type, float a_width);
} // namespace sosr::ui::condition_editor
