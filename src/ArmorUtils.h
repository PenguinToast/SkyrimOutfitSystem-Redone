#pragma once

#include <RE/Skyrim.h>

#include <sstream>

namespace sosr::armor {
std::string FormatFormID(RE::FormID a_formID);
std::string GetDisplayName(const RE::TESForm *a_form);
std::string JoinStrings(const std::vector<std::string> &a_values);
std::vector<std::string> GetArmorSlotLabels(std::uint64_t a_slotMask);
std::string GetPluginName(const RE::TESForm *a_form);
std::string GetFormIdentifier(const RE::TESForm *a_form);
std::string GetReplacementIdentifier(const RE::TESObjectARMO *a_armor,
                                     const RE::TESObjectARMA *a_armorAddon);

template <class T = RE::TESForm>
[[nodiscard]] auto LookupByIdentifier(const std::string &a_identifier) -> T * {
  std::istringstream stream(a_identifier);
  std::string plugin;
  std::string localFormIDText;
  if (!std::getline(stream, plugin, '|') ||
      !std::getline(stream, localFormIDText) || plugin.empty() ||
      localFormIDText.empty()) {
    return nullptr;
  }

  RE::FormID localFormID = 0;
  std::istringstream(localFormIDText) >> std::hex >> localFormID;

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return nullptr;
  }

  auto *form = dataHandler->LookupForm(localFormID, plugin);
  return form ? form->As<T>() : nullptr;
}
} // namespace sosr::armor
