#include "Menu.h"

#include "PlayerInventory.h"
#include "ui/catalog/Widgets.h"

namespace {
constexpr std::string_view kFavoritePrefix = "\xEE\x83\xB5 ";
}

namespace sosr {
void Menu::ClearCatalogSelection() {
  selectedCatalogKey_.clear();
  workbench_.ClearPreview();
}

std::string Menu::BuildFavoriteKey(const BrowserTab a_tab,
                                   const std::string_view a_id) const {
  std::string prefix;
  switch (a_tab) {
  case BrowserTab::Gear:
    prefix = "gear:";
    break;
  case BrowserTab::Outfits:
    prefix = "outfit:";
    break;
  case BrowserTab::Kits:
    prefix = "kit:";
    break;
  case BrowserTab::Slots:
    prefix = "slot:";
    break;
  case BrowserTab::Conditions:
    prefix = "condition:";
    break;
  case BrowserTab::Options:
    prefix = "options:";
    break;
  }

  return prefix + std::string(a_id);
}

bool Menu::IsFavorite(const BrowserTab a_tab,
                      const std::string_view a_id) const {
  return favoriteKeys_.contains(BuildFavoriteKey(a_tab, a_id));
}

void Menu::SetFavorite(const BrowserTab a_tab, const std::string_view a_id,
                       const bool a_favorite) {
  const auto key = BuildFavoriteKey(a_tab, a_id);
  if (a_favorite) {
    favoriteKeys_.insert(key);
  } else {
    favoriteKeys_.erase(key);
    if (favoritesOnly_ && activeTab_ == a_tab && selectedCatalogKey_ == a_id) {
      ClearCatalogSelection();
    }
  }
  SaveFavorites();
}

std::string Menu::BuildFavoriteLabel(const std::string_view a_name,
                                     const bool a_favorite) const {
  if (!a_favorite) {
    return std::string(a_name);
  }

  return std::string(kFavoritePrefix) + std::string(a_name);
}

void Menu::QueueCatalogRefresh(const CatalogRefreshMode a_mode) {
  catalogRefreshQueued_ = true;
  queuedCatalogRefreshMode_ = a_mode;
  if (a_mode == CatalogRefreshMode::Full) {
    catalogInitialized_ = false;
    pendingCatalogSelectionAfterRefresh_.clear();
    ClearCatalogSelection();
  }
}

void Menu::UpdateCatalogRefresh() {
  auto &catalog = EquipmentCatalog::Get();
  if (catalogRefreshQueued_ && !catalog.IsRefreshing()) {
    catalog.StartRefreshFromGame(queuedCatalogRefreshMode_ ==
                                         CatalogRefreshMode::KitsOnly
                                     ? EquipmentCatalog::RefreshMode::KitsOnly
                                     : EquipmentCatalog::RefreshMode::Full);
    catalogRefreshQueued_ = false;
  }

  if (!catalog.IsRefreshing()) {
    return;
  }

  if (!catalog.ContinueRefreshFromGame(16.0)) {
    catalogInitialized_ = true;
    if (!pendingCatalogSelectionAfterRefresh_.empty()) {
      selectedCatalogKey_ = pendingCatalogSelectionAfterRefresh_;
      pendingCatalogSelectionAfterRefresh_.clear();
    }
  }
}

void Menu::DrawCatalogLoadingPane() const {
  const auto &catalog = EquipmentCatalog::Get();
  const auto avail = ImGui::GetContentRegionAvail();
  const auto barWidth = (std::min)(avail.x, 420.0f);
  const auto progress = std::clamp(catalog.GetRefreshProgress(), 0.0f, 1.0f);
  const auto status = catalog.GetRefreshStatus();

  ImGui::Dummy(ImVec2(0.0f, (std::max)(avail.y * 0.30f, 0.0f)));
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + barWidth);
  ImGui::TextUnformatted(status.data(), status.data() + status.size());
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
  ImGui::ProgressBar(progress, ImVec2(barWidth, 0.0f));
  ImGui::Spacing();
  ImGui::TextDisabled("%.0f%%", progress * 100.0f);
}

void Menu::AddGearEntryToWorkbench(const GearEntry &a_entry) {
  const auto visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  workbench_.AddCatalogSelectionToWorkbench(
      std::vector<RE::FormID>{a_entry.formID}, &visibleRowIndices);
}

void Menu::AddOutfitEntryToWorkbench(const OutfitEntry &a_entry,
                                     const bool a_replaceExisting) {
  const auto visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  if (a_replaceExisting) {
    workbench_.ReplaceCatalogSelectionInWorkbench(a_entry.armorFormIDs,
                                                  &visibleRowIndices);
  } else {
    workbench_.AddCatalogSelectionToWorkbench(a_entry.armorFormIDs,
                                              &visibleRowIndices);
  }
}

void Menu::PreviewGearEntry(const GearEntry &a_entry) {
  const auto visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  workbench_.ApplyCatalogPreview(
      a_entry.id, std::vector<RE::FormID>{a_entry.formID},
      ResolveWorkbenchPreviewActor(), &visibleRowIndices);
}

void Menu::PreviewOutfitEntry(const OutfitEntry &a_entry) {
  const auto visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  workbench_.ApplyCatalogPreview(a_entry.id, a_entry.armorFormIDs,
                                 ResolveWorkbenchPreviewActor(),
                                 &visibleRowIndices);
}

void Menu::DrawWindow() {
  auto &io = ImGui::GetIO();
  ImGui::SetNextWindowSize(
      ImVec2(io.DisplaySize.x * 0.50f, io.DisplaySize.y * 0.50f),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.50f, io.DisplaySize.y * 0.50f),
      ImGuiCond_FirstUseEver, ImVec2(0.50f, 0.50f));

  bool open = enabled_;
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, windowAlpha_);
  if (!ImGui::Begin("Skyrim Vanity System", &open,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    ImGui::PopStyleVar();
    if (!open) {
      Close();
    }
    return;
  }

  ImGui::TextUnformatted("Vanity outfit browser");
  ImGui::SameLine();
  ImGui::TextDisabled("| %s toggles visibility", GetToggleKeyLabel().c_str());
  const auto optionsButtonWidth = ImGui::CalcTextSize("Options").x +
                                  (ImGui::GetStyle().FramePadding.x * 2.0f);
  const auto browserButtonWidth = ImGui::CalcTextSize("Browser").x +
                                  (ImGui::GetStyle().FramePadding.x * 2.0f);
  const auto buttonsWidth =
      browserButtonWidth + optionsButtonWidth + ImGui::GetStyle().ItemSpacing.x;
  const auto buttonStartX = ImGui::GetWindowContentRegionMax().x - buttonsWidth;
  if (buttonStartX > ImGui::GetCursorPosX()) {
    ImGui::SameLine(buttonStartX);
  } else {
    ImGui::SameLine();
  }
  if (ImGui::Selectable("Browser", activeTab_ != BrowserTab::Options, 0,
                        ImVec2(browserButtonWidth, 0.0f)) &&
      activeTab_ == BrowserTab::Options) {
    activeTab_ = BrowserTab::Gear;
  }
  ImGui::SameLine();
  if (ImGui::Selectable("Options", activeTab_ == BrowserTab::Options, 0,
                        ImVec2(optionsButtonWidth, 0.0f))) {
    activeTab_ = BrowserTab::Options;
  }
  ImGui::Separator();
  const auto davAvailabilityMessage = ui::catalog::GetDavAvailabilityMessage();
  if (davAvailabilityMessage) {
    ui::catalog::DrawDavAvailabilityBanner(*davAvailabilityMessage);
    ImGui::Spacing();
  }

  if (activeTab_ == BrowserTab::Options) {
    DrawOptionsTab();
  } else {
    UpdateCatalogRefresh();
    const auto applySelectedPreview = [&]() {
      if (EquipmentCatalog::Get().IsRefreshing()) {
        return;
      }
      if (!previewSelected_ || selectedCatalogKey_.empty()) {
        return;
      }

      if (activeTab_ == BrowserTab::Gear) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetGear(), selectedCatalogKey_,
            [](const GearEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetGear().end()) {
          PreviewGearEntry(*entry);
        }
      } else if (activeTab_ == BrowserTab::Outfits) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetOutfits(), selectedCatalogKey_,
            [](const OutfitEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetOutfits().end()) {
          PreviewOutfitEntry(*entry);
        }
      } else if (activeTab_ == BrowserTab::Kits) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetKits(), selectedCatalogKey_,
            [](const KitEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetKits().end()) {
          PreviewKitEntry(*entry);
        }
      } else if (activeTab_ == BrowserTab::Slots ||
                 activeTab_ == BrowserTab::Conditions) {
        workbench_.ClearPreview();
      }
    };

    if (ImGui::BeginTabBar("##catalog-tabs")) {
      const bool gearTabOpen = ImGui::BeginTabItem("Gear");
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:gear-tab", ui::catalog::IsDelayedHover(),
          {"Use this tab to override a specific equipped gear piece.",
           "Browse individual armor pieces from the equipment catalog.",
           "Double-click to add an override using Skyrim Vanity System's "
           "default target selection, or use the context menu to add a new "
           "workbench row instead."});
      if (gearTabOpen) {
        if (activeTab_ != BrowserTab::Gear) {
          ClearCatalogSelection();
        }
        activeTab_ = BrowserTab::Gear;
        ImGui::EndTabItem();
      }

      const bool outfitsTabOpen = ImGui::BeginTabItem("Outfits");
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:outfits-tab", ui::catalog::IsDelayedHover(),
          {"Browse full outfits from plugins in the catalog.",
           "Double-click to replace matching row overrides so the result "
           "matches the preview. The context menu also offers the old append "
           "behavior, or you can add the outfit as workbench rows instead."});
      if (outfitsTabOpen) {
        if (activeTab_ != BrowserTab::Outfits) {
          ClearCatalogSelection();
        }
        activeTab_ = BrowserTab::Outfits;
        ImGui::EndTabItem();
      }

      const bool kitsTabOpen = ImGui::BeginTabItem("Kits");
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:kits-tab", ui::catalog::IsDelayedHover(),
          {"Browse Mod Explorer kits loaded from "
           "data/interface/modex/user/kits.",
           "Kits behave like outfits: double-click replaces matching row "
           "overrides so the result matches the preview. The context menu also "
           "offers the old append behavior, can add rows to the workbench, "
           "and can delete kits.",
           "Kits that refer to non-existent items are not shown."});
      if (kitsTabOpen) {
        if (activeTab_ != BrowserTab::Kits) {
          ClearCatalogSelection();
        }
        activeTab_ = BrowserTab::Kits;
        ImGui::EndTabItem();
      }

      const bool slotsTabOpen = ImGui::BeginTabItem("Equipment Slots");
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:slots-tab", ui::catalog::IsDelayedHover(),
          {"Use this tab to override a specific equipment slot no matter "
           "which armor you have equipped there, as long as something is "
           "equipped in that slot.",
           "Browse slot-based overrides that target armor addon slots.",
           "Armor addons are sub-components of an armor, and Dynamic Armor "
           "Variants Extended resolves slot overrides against the union of "
           "those addon slots rather than only the slots declared on the "
           "armor form itself.",
           "Double-click to add a slot row to the workbench, or use the "
           "context menu to add it manually."});
      if (slotsTabOpen) {
        if (activeTab_ != BrowserTab::Slots) {
          ClearCatalogSelection();
        }
        activeTab_ = BrowserTab::Slots;
        ImGui::EndTabItem();
      }

      const bool conditionsTabOpen = ImGui::BeginTabItem("Conditions");
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:conditions-tab", ui::catalog::IsDelayedHover(),
          {"Use this tab to define reusable condition sets for Dynamic Armor "
           "Variants Extended.",
           "Conditions are built from condition functions joined by AND and "
           "OR operators, plus a shared display color so you can recognize "
           "them later.",
           "Double-click a condition to edit it, or use Add New to create a "
           "fresh one."});
      if (conditionsTabOpen) {
        if (activeTab_ != BrowserTab::Conditions) {
          ClearCatalogSelection();
        }
        activeTab_ = BrowserTab::Conditions;
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::Separator();
    DrawCatalogFilters();
    if (activeTab_ != BrowserTab::Slots &&
        activeTab_ != BrowserTab::Conditions) {
      if (ImGui::Checkbox("Favorites Only", &favoritesOnly_) &&
          favoritesOnly_ && !selectedCatalogKey_.empty() &&
          !IsFavorite(activeTab_, selectedCatalogKey_)) {
        ClearCatalogSelection();
      }
    }
    if (activeTab_ == BrowserTab::Gear) {
      ImGui::SameLine();
      if (ImGui::Checkbox("Inventory Only", &inventoryOnly_) &&
          inventoryOnly_ && !selectedCatalogKey_.empty()) {
        const auto &catalog = EquipmentCatalog::Get().GetGear();
        const auto selectedIt =
            std::ranges::find(catalog, selectedCatalogKey_, &GearEntry::id);
        const auto inventoryFormIDs =
            player_inventory::GetInventoryArmorFormIDs();
        if (selectedIt != catalog.end() &&
            !inventoryFormIDs.contains(selectedIt->formID)) {
          ClearCatalogSelection();
        }
      }
    }
    if (activeTab_ == BrowserTab::Slots) {
      ImGui::Checkbox("Show all", &showAllSlots_);
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:slots-show-all", ui::catalog::IsDelayedHover(0.55f),
          {"When unchecked, only slots that currently have equipped items are "
           "shown.",
           "Enable this to browse every supported equipment slot, including "
           "slots that are currently empty."});
    }
    if (activeTab_ != BrowserTab::Slots &&
        activeTab_ != BrowserTab::Conditions) {
      ImGui::SameLine();
      if (ImGui::Checkbox("Preview Selected", &previewSelected_)) {
        if (!previewSelected_) {
          workbench_.ClearPreview();
        } else {
          applySelectedPreview();
        }
      }
    }

    if (ImGui::BeginTable("##browser-layout", 2,
                          ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
      ImGui::TableSetupColumn("Catalog", ImGuiTableColumnFlags_WidthStretch,
                              1.20f);
      ImGui::TableSetupColumn("Variants", ImGuiTableColumnFlags_WidthStretch,
                              0.95f);
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      if (ImGui::BeginChild("##catalog-pane", ImVec2(0.0f, 0.0f),
                            ImGuiChildFlags_Borders)) {
        bool catalogRowClicked = false;
        if (activeTab_ != BrowserTab::Conditions &&
            EquipmentCatalog::Get().IsRefreshing()) {
          DrawCatalogLoadingPane();
        } else {
          if (activeTab_ == BrowserTab::Gear) {
            catalogRowClicked = DrawGearTab();
          } else if (activeTab_ == BrowserTab::Outfits) {
            catalogRowClicked = DrawOutfitTab();
          } else if (activeTab_ == BrowserTab::Kits) {
            catalogRowClicked = DrawKitTab();
          } else if (activeTab_ == BrowserTab::Conditions) {
            catalogRowClicked = DrawConditionTab();
          } else {
            catalogRowClicked = DrawSlotTab();
          }

          if (!selectedCatalogKey_.empty() &&
              ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
              !catalogRowClicked) {
            ClearCatalogSelection();
          }
        }
      }
      ImGui::EndChild();

      ImGui::TableSetColumnIndex(1);
      if (ImGui::BeginChild("##variant-pane", ImVec2(0.0f, 0.0f),
                            ImGuiChildFlags_Borders)) {
        DrawVariantWorkbenchPane();
      }
      ImGui::EndChild();

      ImGui::EndTable();
    }

    workbench_.SyncDynamicArmorVariantsExtended(conditions_);
  }

  DrawCreateKitDialog();
  DrawDeleteKitDialog();
  DrawConditionEditorDialog();

  ImGui::End();
  ImGui::PopStyleVar();

  if (!open) {
    Close();
  }
}
} // namespace sosr
