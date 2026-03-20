#pragma once

#include <RE/Skyrim.h>

namespace sosng::armor {
std::string FormatFormID(RE::FormID a_formID);
std::string GetDisplayName(const RE::TESForm *a_form);
std::string JoinStrings(const std::vector<std::string> &a_values);
std::vector<std::string> GetArmorSlotLabels(std::uint64_t a_slotMask);
std::string GetPluginName(const RE::TESForm *a_form);
std::string GetFormIdentifier(const RE::TESForm *a_form);
std::string GetReplacementIdentifier(const RE::TESObjectARMO *a_armor,
                                     const RE::TESObjectARMA *a_armorAddon);
} // namespace sosng::armor
