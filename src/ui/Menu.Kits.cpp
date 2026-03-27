#include "Menu.h"

#include "ArmorUtils.h"
#include "ui/components/EditableCombo.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
constexpr auto kModexKitDirectory = "data/interface/modex/user/kits";

std::string TrimText(std::string_view a_text) {
  std::size_t start = 0;
  while (start < a_text.size() &&
         std::isspace(static_cast<unsigned char>(a_text[start])) != 0) {
    ++start;
  }

  std::size_t end = a_text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(a_text[end - 1])) != 0) {
    --end;
  }

  return std::string(a_text.substr(start, end - start));
}

std::string NormalizeKitCollection(std::string_view a_collection) {
  auto normalized = TrimText(a_collection);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  while (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  while (!normalized.empty() && normalized.back() == '/') {
    normalized.pop_back();
  }
  return normalized;
}

int CompareTextInsensitive(std::string_view a_left, std::string_view a_right) {
  const auto leftSize = a_left.size();
  const auto rightSize = a_right.size();
  const auto count = (std::min)(leftSize, rightSize);

  for (std::size_t index = 0; index < count; ++index) {
    const auto left = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_left[index])));
    const auto right = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_right[index])));
    if (left < right) {
      return -1;
    }
    if (left > right) {
      return 1;
    }
  }

  if (leftSize < rightSize) {
    return -1;
  }
  if (leftSize > rightSize) {
    return 1;
  }
  return 0;
}
} // namespace

namespace sosr {
void Menu::AddKitEntryToWorkbench(const KitEntry &a_entry,
                                  const bool a_replaceExisting) {
  if (a_replaceExisting) {
    workbench_.ReplaceCatalogSelectionInWorkbench(a_entry.armorFormIDs);
  } else {
    workbench_.AddCatalogSelectionToWorkbench(a_entry.armorFormIDs);
  }
}

void Menu::OpenCreateKitDialog(const KitCreationSource a_source) {
  pendingKitFormIDs_.clear();
  if (a_source == KitCreationSource::Equipped) {
    pendingKitFormIDs_ = workbench_.CollectEquippedArmorFormIDs();
  } else {
    pendingKitFormIDs_ =
        workbench_.CollectOverrideArmorFormIDsFromEquippedRows();
  }

  if (pendingKitFormIDs_.empty()) {
    return;
  }

  createKitSource_ = a_source;
  pendingKitName_.fill('\0');
  pendingKitCollection_.fill('\0');
  createKitError_.clear();
  openCreateKitDialog_ = true;
}

void Menu::OpenDeleteKitDialog(const KitEntry &a_entry) {
  pendingDeleteKitId_ = a_entry.id;
  pendingDeleteKitName_ = a_entry.name;
  pendingDeleteKitPath_ = a_entry.filepath;
  deleteKitError_.clear();
  openDeleteKitDialog_ = true;
}

bool Menu::SavePendingKit() {
  const auto name = TrimText(pendingKitName_.data());
  if (name.empty()) {
    createKitError_ = "Kit name is required.";
    return false;
  }
  if (name.find_first_of("\"'") != std::string::npos) {
    createKitError_ = "Kit name cannot contain quotes.";
    return false;
  }
  if (name.find_first_of("/\\") != std::string::npos) {
    createKitError_ = "Kit name cannot contain path separators.";
    return false;
  }

  auto collection = NormalizeKitCollection(pendingKitCollection_.data());
  if (collection.find_first_of("\"'") != std::string::npos) {
    createKitError_ = "Collection cannot contain quotes.";
    return false;
  }

  nlohmann::json items = nlohmann::json::object();
  for (const auto formID : pendingKitFormIDs_) {
    const auto *armorForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (!armorForm) {
      continue;
    }

    const auto editorID = armor::GetEditorID(armorForm);
    if (editorID.empty()) {
      createKitError_ = "Cannot save kit because '" +
                        armor::GetDisplayName(armorForm) +
                        "' has no editor ID.";
      return false;
    }

    items[editorID] = {{"Plugin", armor::GetPluginName(armorForm)},
                       {"Name", armor::GetDisplayName(armorForm)},
                       {"Amount", 1},
                       {"Equipped", true}};
  }

  if (items.empty()) {
    createKitError_ = "No valid armor items were available to save.";
    return false;
  }

  const auto &kits = EquipmentCatalog::Get().GetKits();
  const auto existingIt =
      std::ranges::find_if(kits, [&](const KitEntry &a_kit) {
        return CompareTextInsensitive(a_kit.name, name) == 0;
      });

  std::filesystem::path relativePath;
  std::filesystem::path fullPath;
  if (existingIt != kits.end()) {
    relativePath = existingIt->key;
    fullPath = existingIt->filepath;
    collection = existingIt->collection;
  } else {
    relativePath = std::filesystem::path(collection) / (name + ".json");
    fullPath = std::filesystem::path(kModexKitDirectory) / relativePath;
  }

  try {
    std::filesystem::create_directories(fullPath.parent_path());
  } catch (const std::exception &exception) {
    createKitError_ =
        "Failed to create kit directory: " + std::string(exception.what());
    return false;
  }

  nlohmann::json data = nlohmann::json::object();
  data[name] = nlohmann::json::object();
  data[name]["Collection"] = collection;
  data[name]["Description"] = "Created by Skyrim Vanity System.";
  data[name]["Items"] = std::move(items);

  std::ofstream file(fullPath, std::ios::trunc);
  if (!file.is_open()) {
    createKitError_ = "Failed to open kit file for writing.";
    return false;
  }

  file << data.dump(4) << '\n';
  file.close();

  pendingCatalogSelectionAfterRefresh_ = "kit:" + relativePath.generic_string();
  QueueCatalogRefresh(CatalogRefreshMode::KitsOnly);
  selectedCatalogKey_.clear();
  activeTab_ = BrowserTab::Kits;
  openCreateKitDialog_ = false;
  pendingKitFormIDs_.clear();
  createKitError_.clear();
  return true;
}

bool Menu::DeletePendingKit() {
  if (pendingDeleteKitPath_.empty()) {
    deleteKitError_ = "Kit file path is unavailable.";
    return false;
  }

  std::error_code error;
  const auto removed = std::filesystem::remove(
      std::filesystem::path(pendingDeleteKitPath_), error);
  if (error) {
    deleteKitError_ = "Failed to delete kit file: " + error.message();
    return false;
  }
  if (!removed) {
    deleteKitError_ = "Kit file no longer exists.";
    return false;
  }

  favoriteKeys_.erase(BuildFavoriteKey(BrowserTab::Kits, pendingDeleteKitId_));
  SaveFavorites();

  if (selectedCatalogKey_ == pendingDeleteKitId_) {
    ClearCatalogSelection();
  }

  QueueCatalogRefresh(CatalogRefreshMode::KitsOnly);
  activeTab_ = BrowserTab::Kits;
  pendingDeleteKitId_.clear();
  pendingDeleteKitName_.clear();
  pendingDeleteKitPath_.clear();
  deleteKitError_.clear();
  return true;
}

void Menu::PreviewKitEntry(const KitEntry &a_entry) {
  workbench_.ApplyCatalogPreview(a_entry.id, a_entry.armorFormIDs);
}

void Menu::DrawCreateKitDialog() {
  if (openCreateKitDialog_) {
    ImGui::OpenPopup("Create Modex Kit");
    openCreateKitDialog_ = false;
  }

  const auto &catalog = EquipmentCatalog::Get();
  std::vector<std::string> existingNames;
  existingNames.reserve(catalog.GetKits().size());
  for (const auto &kit : catalog.GetKits()) {
    if (std::ranges::find(existingNames, kit.name) == existingNames.end()) {
      existingNames.push_back(kit.name);
    }
  }
  std::ranges::sort(existingNames);

  createKitDialogOpen_ = false;
  if (ImGui::BeginPopupModal("Create Modex Kit", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    createKitDialogOpen_ = true;
    const bool popupAppearing = ImGui::IsWindowAppearing();
    ImGui::TextWrapped(
        "%s",
        createKitSource_ == KitCreationSource::Equipped
            ? "Create a Modex kit from the player's currently equipped armor."
            : "Create a Modex kit from overrides on currently equipped armor "
              "only.");
    ImGui::Separator();
    ImGui::Text("Items: %zu", pendingKitFormIDs_.size());

    constexpr float fieldWidth = 360.0f;
    std::string selectedName;
    ImGui::TextUnformatted("Name");
    if (popupAppearing) {
      ImGui::SetKeyboardFocusHere();
    }
    if (ui::components::DrawEditableDropdown(
            "kit-name", "New or existing kit name", pendingKitName_.data(),
            pendingKitName_.size(), existingNames, fieldWidth, &selectedName,
            false) &&
        !selectedName.empty()) {
      if (const auto it = std::ranges::find_if(catalog.GetKits(),
                                               [&](const KitEntry &a_entry) {
                                                 return a_entry.name ==
                                                        selectedName;
                                               });
          it != catalog.GetKits().end()) {
        std::snprintf(pendingKitCollection_.data(),
                      pendingKitCollection_.size(), "%s",
                      it->collection.c_str());
      }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Collection");
    ui::components::DrawEditableDropdown(
        "kit-collection", "Collection (optional)", pendingKitCollection_.data(),
        pendingKitCollection_.size(), catalog.GetKitCollections(), fieldWidth,
        nullptr, false);

    if (!createKitError_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                             ThemeConfig::GetSingleton()->GetColorU32("WARN")),
                         "%s", createKitError_.c_str());
    }

    ImGui::Spacing();
    const bool requestClose =
        createKitDialogCancelRequested_ ||
        ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteFocused);
    if (ImGui::Button("Save", ImVec2(120.0f, 0.0f))) {
      if (SavePendingKit()) {
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)) || requestClose) {
      createKitError_.clear();
      pendingKitFormIDs_.clear();
      pendingKitName_.fill('\0');
      pendingKitCollection_.fill('\0');
      createKitDialogOpen_ = false;
      createKitDialogCancelRequested_ = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  } else {
    createKitDialogOpen_ = false;
    createKitDialogCancelRequested_ = false;
  }
}

void Menu::DrawDeleteKitDialog() {
  constexpr float kDeleteDialogWidth = 460.0f;

  if (openDeleteKitDialog_) {
    ImGui::OpenPopup("Delete Modex Kit");
    openDeleteKitDialog_ = false;
  }

  const auto closeDialog = [&]() {
    pendingDeleteKitId_.clear();
    pendingDeleteKitName_.clear();
    pendingDeleteKitPath_.clear();
    deleteKitError_.clear();
    deleteKitDialogOpen_ = false;
    deleteKitDialogCancelRequested_ = false;
    ImGui::CloseCurrentPopup();
  };

  deleteKitDialogOpen_ = false;
  if (const auto *viewport = ImGui::GetMainViewport()) {
    ImGui::SetNextWindowSize(ImVec2(kDeleteDialogWidth, 0.0f),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
  }
  if (ImGui::BeginPopupModal("Delete Modex Kit", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    deleteKitDialogOpen_ = true;

    ImGui::TextWrapped("%s",
                       "Are you sure you want to delete this kit? This will "
                       "permanently remove the kit JSON file.");
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::TextUnformatted("Name");
    ImGui::TextWrapped("%s", pendingDeleteKitName_.c_str());
    if (!pendingDeleteKitPath_.empty()) {
      ImGui::Spacing();
      ImGui::TextUnformatted("File");
      ImGui::TextWrapped("%s", pendingDeleteKitPath_.c_str());
    }

    if (!deleteKitError_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                             ThemeConfig::GetSingleton()->GetColorU32("WARN")),
                         "%s", deleteKitError_.c_str());
    }

    ImGui::Spacing();
    const bool requestClose =
        deleteKitDialogCancelRequested_ ||
        ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteFocused);

    ImGui::PushStyleColor(
        ImGuiCol_Button,
        ThemeConfig::GetSingleton()->GetColorU32("DECLINE", 0.90f));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ThemeConfig::GetSingleton()->GetColorU32("DECLINE", 1.00f));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonActive,
        ThemeConfig::GetSingleton()->GetColorU32("DECLINE", 0.80f));
    if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f))) {
      if (DeletePendingKit()) {
        closeDialog();
      }
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)) || requestClose) {
      closeDialog();
    }

    ImGui::EndPopup();
  } else {
    deleteKitDialogOpen_ = false;
    deleteKitDialogCancelRequested_ = false;
  }
}
} // namespace sosr
