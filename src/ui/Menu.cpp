#include "Menu.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "PlayerInventory.h"
#include "imgui_internal.h"
#include "integrations/DynamicArmorVariantsExtendedClient.h"
#include "ui/components/CatalogCollectionTooltip.h"
#include "ui/components/EditableCombo.h"
#include "ui/components/EquipmentWidget.h"
#include "ui/components/PinnableTooltip.h"

#include <functional>
#include <optional>

namespace {
enum class GearColumn : ImGuiID { Name = 1, Plugin };

enum class OutfitColumn : ImGuiID { Name = 1, Plugin, Pieces };

enum class KitColumn : ImGuiID { Name = 1, Collection, Pieces };

constexpr std::string_view kFavoritePrefix = "\xEE\x83\xB5 ";
constexpr std::array<ImWchar, 3> kLucideIconRanges = {0xE038, 0xE63F, 0};
constexpr char kIconEditorId[] = "\xee\x84\x8b";   // ICON_LC_LIST
constexpr char kIconPlugin[] = "\xee\x84\xac";     // ICON_LC_PACKAGE
constexpr char kIconFormId[] = "\xee\x83\xb2";     // ICON_LC_HASH
constexpr char kIconIdentifier[] = "\xee\x84\x87"; // ICON_LC_LINK
constexpr char kIconCollection[] = "\xee\x97\xbf"; // ICON_LC_FOLDER_CODE
constexpr char kIconFile[] = "\xee\x83\x87";       // ICON_LC_FILE_CODE

int CompareText(std::string_view a_left, std::string_view a_right) {
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

std::string TruncateTextToWidth(std::string_view a_text, float a_width) {
  if (a_text.empty() || a_width <= 0.0f) {
    return {};
  }

  if (ImGui::CalcTextSize(a_text.data(), a_text.data() + a_text.size()).x <=
      a_width) {
    return std::string(a_text);
  }

  constexpr std::string_view ellipsis = "...";
  const auto ellipsisWidth =
      ImGui::CalcTextSize(ellipsis.data(), ellipsis.data() + ellipsis.size()).x;
  if (ellipsisWidth >= a_width) {
    return std::string(ellipsis);
  }

  std::string truncated;
  truncated.reserve(a_text.size());
  for (char character : a_text) {
    truncated.push_back(character);
    const auto currentWidth =
        ImGui::CalcTextSize(truncated.data(),
                            truncated.data() + truncated.size())
            .x;
    if ((currentWidth + ellipsisWidth) > a_width) {
      truncated.pop_back();
      break;
    }
  }

  truncated.append(ellipsis);
  return truncated;
}

void DrawOutfitTooltip(const sosr::OutfitEntry &a_outfit,
                       const bool a_hoveredSource) {
  std::vector<sosr::ui::components::CatalogTooltipMetaRow> metaRows;
  if (!a_outfit.editorID.empty()) {
    metaRows.push_back({kIconEditorId, "Editor ID", a_outfit.editorID});
  }
  if (!a_outfit.plugin.empty()) {
    metaRows.push_back({kIconPlugin, "Plugin", a_outfit.plugin});
  }
  metaRows.push_back(
      {kIconFormId, "Form ID", sosr::armor::FormatFormID(a_outfit.formID)});
  if (const auto *form = RE::TESForm::LookupByID(a_outfit.formID)) {
    if (const auto identifier = sosr::armor::GetFormIdentifier(form);
        !identifier.empty()) {
      metaRows.push_back({kIconIdentifier, "Identifier", identifier});
    }
  }

  sosr::ui::components::DrawCatalogCollectionTooltip(
      "outfit:" + a_outfit.id, a_hoveredSource, a_outfit.name, metaRows,
      a_outfit.itemTree);
}

void DrawKitTooltip(const sosr::KitEntry &a_kit, const bool a_hoveredSource) {
  std::vector<sosr::ui::components::CatalogTooltipMetaRow> metaRows;
  metaRows.push_back({kIconCollection, "Collection",
                      a_kit.collection.empty() ? "Root" : a_kit.collection});
  if (!a_kit.filepath.empty()) {
    metaRows.push_back({kIconFile, "File", a_kit.filepath});
  }
  metaRows.push_back({kIconIdentifier, "Identifier", a_kit.id});

  sosr::ui::components::DrawCatalogCollectionTooltip(
      "kit:" + a_kit.id, a_hoveredSource, a_kit.name, metaRows, a_kit.itemTree);
}

void DrawSimplePinnableTooltip(const std::string_view a_id,
                               const bool a_hoveredSource,
                               const std::function<void()> &a_drawBody) {
  sosr::ui::components::DrawPinnableTooltip(a_id, a_hoveredSource, a_drawBody);
}

void DrawCatalogTabHelpTooltip(
    const std::string_view a_id, const bool a_hoveredSource,
    const std::initializer_list<const char *> a_lines) {
  const auto mousePos = ImGui::GetIO().MousePos;
  sosr::ui::components::HoveredTooltipOptions tooltipOptions;
  tooltipOptions.useCustomPlacement = true;
  tooltipOptions.pos = ImVec2(mousePos.x - 2.0f, mousePos.y + 12.0f);
  tooltipOptions.pivot = ImVec2(1.0f, 0.0f);
  sosr::ui::components::DrawPinnableTooltip(
      a_id, a_hoveredSource,
      [&]() {
        bool first = true;
        for (const auto *line : a_lines) {
          if (!first) {
            ImGui::Spacing();
          }
          ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f);
          ImGui::TextUnformatted(line);
          ImGui::PopTextWrapPos();
          first = false;
        }
      },
      tooltipOptions);
}

bool IsDelayedHover(const float a_delaySeconds = 0.45f) {
  return ImGui::IsItemHovered() &&
         ImGui::GetCurrentContext()->HoveredIdTimer >= a_delaySeconds;
}

std::optional<std::string> GetDavAvailabilityMessage() {
  return sosr::integrations::DynamicArmorVariantsExtendedClient::
      GetAvailabilityMessage();
}

void DrawDavAvailabilityBanner(const std::string_view a_message) {
  if (a_message.empty()) {
    return;
  }

  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(88, 30, 30, 180));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(196, 78, 78, 255));
  if (ImGui::BeginChild("##dav-unavailable-banner", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_Borders |
                            ImGuiChildFlags_AutoResizeY)) {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 205, 205, 255));
    ImGui::TextWrapped("%.*s", static_cast<int>(a_message.size()),
                       a_message.data());
    ImGui::PopStyleColor();
  }
  ImGui::EndChild();
  ImGui::PopStyleColor(2);
}

} // namespace

namespace sosr {
ui::components::EquipmentWidgetResult
Menu::DrawCatalogDragWidget(const workbench::EquipmentWidgetItem &a_item,
                            const DragSourceKind a_sourceKind) {
  const auto widgetResult =
      ui::components::DrawEquipmentWidget(a_item.key.c_str(), a_item);
  if (!ImGui::BeginDragDropSource()) {
    return widgetResult;
  }

  DraggedEquipmentPayload payload{};
  payload.sourceKind = static_cast<std::uint32_t>(a_sourceKind);
  payload.rowIndex = -1;
  payload.itemIndex = -1;
  payload.formID = a_item.formID;
  payload.slotMask = a_item.slotMask;
  ImGui::SetDragDropPayload("SOSR_VARIANT_ITEM", &payload, sizeof(payload));
  ImGui::TextUnformatted(a_item.name.c_str());
  ImGui::Text("%s", a_item.slotText.c_str());
  ImGui::EndDragDropSource();
  return widgetResult;
}

Menu *Menu::GetSingleton() {
  static Menu singleton;
  return std::addressof(singleton);
}

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
  workbench_.ApplyCatalogPreview(a_entry.id,
                                 std::vector<RE::FormID>{a_entry.formID},
                                 ResolveWorkbenchPreviewActor(),
                                 &visibleRowIndices);
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
  const auto davAvailabilityMessage = GetDavAvailabilityMessage();
  if (davAvailabilityMessage) {
    DrawDavAvailabilityBanner(*davAvailabilityMessage);
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
      DrawCatalogTabHelpTooltip(
          "catalog:gear-tab", IsDelayedHover(),
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
      DrawCatalogTabHelpTooltip(
          "catalog:outfits-tab", IsDelayedHover(),
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
      DrawCatalogTabHelpTooltip(
          "catalog:kits-tab", IsDelayedHover(),
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
      DrawCatalogTabHelpTooltip(
          "catalog:slots-tab", IsDelayedHover(),
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
      DrawCatalogTabHelpTooltip(
          "catalog:conditions-tab", IsDelayedHover(),
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
      DrawCatalogTabHelpTooltip(
          "catalog:slots-show-all", IsDelayedHover(0.55f),
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

bool Menu::DrawGearTab() {
  auto rows = BuildFilteredGear();
  ImGui::Text("Results: %zu", rows.size());
  ImGui::SameLine();
  ImGui::Text("| Equipped rows: %zu", workbench_.GetRowCount());
  return DrawGearCatalogTable(rows);
}

bool Menu::DrawGearCatalogTable(const std::vector<const GearEntry *> &a_rows) {
  bool rowClicked = false;
  if (ImGui::BeginTable("##gear-table", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 0.0f))) {
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Name));
    ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Plugin));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    auto rows = a_rows;
    SortGearRows(rows, ImGui::TableGetSortSpecs());

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        const auto &entry = *rows[static_cast<std::size_t>(rowIndex)];
        const auto favorite = IsFavorite(BrowserTab::Gear, entry.id);
        workbench::EquipmentWidgetItem item{};
        if (!workbench_.BuildCatalogItem(entry.formID, item)) {
          item.formID = entry.formID;
          item.key = "catalog:" + entry.id;
          item.name = entry.name;
        }
        item.name = BuildFavoriteLabel(item.name, favorite);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected =
            selectedCatalogKey_ == entry.id &&
            (!previewSelected_ || workbench_.IsPreviewingSelection(entry.id));
        const bool clicked = ImGui::Selectable(
            ("##catalog-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        if (ImGui::BeginPopupContextItem()) {
          const auto favoriteLabel =
              favorite ? "Remove from Favorites" : "Add to Favorites";
          if (ImGui::MenuItem(favoriteLabel)) {
            SetFavorite(BrowserTab::Gear, entry.id, !favorite);
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add to Workbench")) {
            workbench_.AddCatalogSelectionAsRows(
                std::vector<RE::FormID>{entry.formID},
                ResolveNewWorkbenchRowConditionId());
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add Override")) {
            AddGearEntryToWorkbench(entry);
          }
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);
        const auto widgetResult =
            DrawCatalogDragWidget(item, DragSourceKind::Catalog);

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(entry.plugin.data());

        if (selected) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY", 0.40f));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("TABLE_HOVER", 0.12f));
        }

        if (clicked || widgetResult.clicked) {
          rowClicked = true;
          if (selectedCatalogKey_ == entry.id) {
            ClearCatalogSelection();
          } else {
            selectedCatalogKey_ = entry.id;
            if (previewSelected_) {
              PreviewGearEntry(entry);
            } else {
              workbench_.ClearPreview();
            }
          }
        }

        if ((rowHovered &&
             ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ||
            widgetResult.doubleClicked) {
          rowClicked = true;
          AddGearEntryToWorkbench(entry);
        }
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}

bool Menu::DrawOutfitTab() {
  auto rows = BuildFilteredOutfits();
  ImGui::Text("Results: %zu", rows.size());
  bool rowClicked = false;

  const auto tableHeight = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginTable("##outfit-table", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, tableHeight))) {
    ImGui::TableSetupColumn("Outfit", ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Name));
    ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Plugin));
    ImGui::TableSetupColumn("Pieces",
                            ImGuiTableColumnFlags_PreferSortDescending, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Pieces));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    SortOutfitRows(rows, ImGui::TableGetSortSpecs());

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        const auto &outfit = *rows[static_cast<std::size_t>(rowIndex)];
        const auto favorite = IsFavorite(BrowserTab::Outfits, outfit.id);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected =
            selectedCatalogKey_ == outfit.id &&
            (!previewSelected_ || workbench_.IsPreviewingSelection(outfit.id));
        const bool clicked = ImGui::Selectable(
            ("##outfit-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        if (ImGui::BeginPopupContextItem()) {
          const auto favoriteLabel =
              favorite ? "Remove from Favorites" : "Add to Favorites";
          if (ImGui::MenuItem(favoriteLabel)) {
            SetFavorite(BrowserTab::Outfits, outfit.id, !favorite);
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add to Workbench")) {
            workbench_.AddCatalogSelectionAsRows(
                outfit.armorFormIDs, ResolveNewWorkbenchRowConditionId());
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add Override")) {
            AddOutfitEntryToWorkbench(outfit, true);
          }
          if (ImGui::MenuItem("Append Overrides")) {
            AddOutfitEntryToWorkbench(outfit, false);
          }
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);

        if (selected) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY", 0.40f));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("TABLE_HOVER", 0.12f));
        }

        if (clicked) {
          rowClicked = true;
          if (selectedCatalogKey_ == outfit.id) {
            ClearCatalogSelection();
          } else {
            selectedCatalogKey_ = outfit.id;
            if (previewSelected_) {
              PreviewOutfitEntry(outfit);
            } else {
              workbench_.ClearPreview();
            }
          }
        }

        if (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          rowClicked = true;
          AddOutfitEntryToWorkbench(outfit, true);
        }
        if (!ImGui::IsDragDropActive() &&
            ui::components::ShouldDrawPinnableTooltip("outfit:" + outfit.id,
                                                      rowHovered)) {
          DrawOutfitTooltip(outfit, rowHovered);
        }

        const auto displayName = BuildFavoriteLabel(outfit.name, favorite);
        ImGui::TextUnformatted(displayName.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(outfit.plugin.data());

        ImGui::TableSetColumnIndex(2);
        {
          const auto availableWidth = ImGui::GetContentRegionAvail().x;
          const auto displayText =
              TruncateTextToWidth(outfit.piecesText, availableWidth);
          ImGui::TextUnformatted(displayText.c_str());
        }
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}

bool Menu::DrawKitTab() {
  auto rows = BuildFilteredKits();
  ImGui::Text("Results: %zu", rows.size());
  bool rowClicked = false;

  const auto tableHeight = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginTable("##kit-table", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, tableHeight))) {
    ImGui::TableSetupColumn("Kit", ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(KitColumn::Name));
    ImGui::TableSetupColumn("Collection", ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(KitColumn::Collection));
    ImGui::TableSetupColumn("Pieces",
                            ImGuiTableColumnFlags_PreferSortDescending, 0.0f,
                            static_cast<ImGuiID>(KitColumn::Pieces));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    SortKitRows(rows, ImGui::TableGetSortSpecs());

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        const auto &kit = *rows[static_cast<std::size_t>(rowIndex)];
        const auto favorite = IsFavorite(BrowserTab::Kits, kit.id);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected =
            selectedCatalogKey_ == kit.id &&
            (!previewSelected_ || workbench_.IsPreviewingSelection(kit.id));
        const bool clicked = ImGui::Selectable(
            ("##kit-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        if (ImGui::BeginPopupContextItem()) {
          const auto favoriteLabel =
              favorite ? "Remove from Favorites" : "Add to Favorites";
          if (ImGui::MenuItem(favoriteLabel)) {
            SetFavorite(BrowserTab::Kits, kit.id, !favorite);
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add to Workbench")) {
            workbench_.AddCatalogSelectionAsRows(
                kit.armorFormIDs, ResolveNewWorkbenchRowConditionId());
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add Override")) {
            AddKitEntryToWorkbench(kit, true);
          }
          if (ImGui::MenuItem("Append Overrides")) {
            AddKitEntryToWorkbench(kit, false);
          }
          ImGui::Separator();
          ImGui::PushStyleColor(
              ImGuiCol_Text,
              ImGui::ColorConvertU32ToFloat4(
                  ThemeConfig::GetSingleton()->GetColorU32("DECLINE")));
          if (ImGui::MenuItem("Delete Kit")) {
            OpenDeleteKitDialog(kit);
          }
          ImGui::PopStyleColor();
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);

        if (selected) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY", 0.40f));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("TABLE_HOVER", 0.12f));
        }

        if (clicked) {
          rowClicked = true;
          if (selectedCatalogKey_ == kit.id) {
            ClearCatalogSelection();
          } else {
            selectedCatalogKey_ = kit.id;
            if (previewSelected_) {
              PreviewKitEntry(kit);
            } else {
              workbench_.ClearPreview();
            }
          }
        }

        if (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          rowClicked = true;
          AddKitEntryToWorkbench(kit, true);
        }
        if (!ImGui::IsDragDropActive() &&
            ui::components::ShouldDrawPinnableTooltip("kit:" + kit.id,
                                                      rowHovered)) {
          DrawKitTooltip(kit, rowHovered);
        }

        const auto displayName = BuildFavoriteLabel(kit.name, favorite);
        ImGui::TextUnformatted(displayName.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(kit.collection.empty() ? "Root"
                                                      : kit.collection.c_str());

        ImGui::TableSetColumnIndex(2);
        const auto availableWidth = ImGui::GetContentRegionAvail().x;
        const auto displayText =
            TruncateTextToWidth(kit.piecesText, availableWidth);
        ImGui::TextUnformatted(displayText.c_str());
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}

} // namespace sosr
