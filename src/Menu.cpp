#include "Menu.h"

#include "InputManager.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
constexpr auto kSettingsDirectory = "Data/SKSE/Plugins/SkyrimOutfitSystemNG";
constexpr auto kImGuiIniFilename = "imgui.ini";
constexpr auto kUserSettingsFilename = "settings.json";
constexpr auto kDefaultFontPath =
    "Data/Interface/SkyrimOutfitSystemNG/fonts/Ubuntu-R.ttf";
constexpr int kDefaultFontSizePixels = 16;
constexpr int kMinFontSizePixels = 8;
constexpr int kMaxFontSizePixels = 28;

enum class GearColumn : ImGuiID { Name = 1, Plugin, Slot };

enum class OutfitColumn : ImGuiID { Name = 1, Plugin, Pieces };

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

} // namespace

namespace sosng {
Menu *Menu::GetSingleton() {
  static Menu singleton;
  return std::addressof(singleton);
}

void Menu::Init(IDXGISwapChain *a_swapChain, ID3D11Device *a_device,
                ID3D11DeviceContext *a_context) {
  if (initialized_) {
    return;
  }

  settingsDirectory_ = kSettingsDirectory;
  imguiIniPath_ =
      (std::filesystem::path(settingsDirectory_) / kImGuiIniFilename).string();
  userSettingsPath_ =
      (std::filesystem::path(settingsDirectory_) / kUserSettingsFilename)
          .string();
  LoadUserSettings();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  auto &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = imguiIniPath_.c_str();
  io.LogFilename = nullptr;
  ImGui::GetStyle().FontScaleMain = 1.0f;
  pendingFontSizePixels_ = fontSizePixels_;
  RebuildFontAtlas();

  DXGI_SWAP_CHAIN_DESC description{};
  a_swapChain->GetDesc(&description);

  ImGui_ImplWin32_Init(description.OutputWindow);
  ImGui_ImplDX11_Init(a_device, a_context);

  device_ = a_device;
  context_ = a_context;

  ApplyStyle();
  initialized_ = true;

  logger::info("Initialized Dear ImGui menu");
}

void Menu::LoadUserSettings() {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create settings directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  std::ifstream input(userSettingsPath_);
  if (!input.is_open()) {
    SaveUserSettings();
    return;
  }

  try {
    const auto json = nlohmann::json::parse(input, nullptr, true, true);
    fontSizePixels_ = std::clamp(json.value("fontSizePx", fontSizePixels_),
                                 kMinFontSizePixels, kMaxFontSizePixels);
    pendingFontSizePixels_ = fontSizePixels_;
  } catch (const std::exception &exception) {
    logger::warn("Failed to parse user settings from {}: {}", userSettingsPath_,
                 exception.what());
  }
}

void Menu::SaveUserSettings() const {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create settings directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  std::ofstream output(userSettingsPath_, std::ios::trunc);
  if (!output.is_open()) {
    logger::error("Failed to write user settings to {}", userSettingsPath_);
    return;
  }

  const nlohmann::json json = {{"fontSizePx", fontSizePixels_}};

  output << json.dump(2) << '\n';
}

void Menu::RebuildFontAtlas() {
  auto &io = ImGui::GetIO();
  auto &style = ImGui::GetStyle();
  ImFontConfig fontConfig{};
  fontConfig.SizePixels = static_cast<float>(fontSizePixels_);
  fontConfig.OversampleH = 1;
  fontConfig.OversampleV = 1;
  fontConfig.PixelSnapH = true;

  io.Fonts->Clear();
  io.FontDefault = nullptr;
  if (std::filesystem::exists(kDefaultFontPath)) {
    io.FontDefault = io.Fonts->AddFontFromFileTTF(
        kDefaultFontPath, static_cast<float>(fontSizePixels_), &fontConfig);
  }
  if (!io.FontDefault) {
    io.FontDefault = io.Fonts->AddFontDefaultVector(&fontConfig);
  }
  io.Fonts->Build();
  style.FontScaleMain = 1.0f;
  style._NextFrameFontSizeBase = static_cast<float>(fontSizePixels_);
  style.FontSizeBase = static_cast<float>(fontSizePixels_);

  if (initialized_) {
    ImGui_ImplDX11_InvalidateDeviceObjects();
  }
}

void Menu::ApplyStyle() {
  ImGui::StyleColorsDark();
  auto &style = ImGui::GetStyle();
  style.WindowRounding = 12.0f;
  style.FrameRounding = 8.0f;
  style.PopupRounding = 8.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 8.0f;
  style.FrameBorderSize = 1.0f;
  style.WindowBorderSize = 1.0f;
  style.WindowPadding = {14.0f, 14.0f};
  style.ItemSpacing = {10.0f, 8.0f};
  style.CellPadding = {8.0f, 6.0f};

  auto &colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.11f, 0.96f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.12f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.17f, 0.21f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.20f, 0.28f, 0.37f, 0.70f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.35f, 0.47f, 0.85f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.39f, 0.54f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.22f, 0.30f, 0.40f, 0.75f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.29f, 0.40f, 0.53f, 0.92f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.32f, 0.45f, 0.60f, 1.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.15f, 0.18f, 0.94f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.21f, 0.25f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.21f, 0.24f, 0.29f, 1.00f);
  colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.15f, 0.18f, 0.94f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.33f, 0.45f, 0.96f);
  colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.28f, 0.37f, 0.96f);
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
}

void Menu::Open() {
  if (!initialized_) {
    return;
  }

  auto &io = ImGui::GetIO();
  io.MouseDrawCursor = true;
  io.ClearInputKeys();
  enabled_ = true;
}

void Menu::Close() {
  if (!initialized_) {
    return;
  }

  ClearCatalogSelection();

  auto &io = ImGui::GetIO();
  io.MouseDrawCursor = false;
  io.ClearInputKeys();
  if (io.IniFilename) {
    ImGui::SaveIniSettingsToDisk(io.IniFilename);
  }
  SaveUserSettings();
  enabled_ = false;
}

void Menu::Toggle() {
  if (enabled_) {
    Close();
  } else {
    Open();
  }
}

void Menu::ClearCatalogSelection() {
  selectedCatalogFormID_ = 0;
  workbench_.ClearPreview();
}

void Menu::Draw() {
  InputManager::GetSingleton()->ProcessInputEvents();

  if (!initialized_ || !enabled_) {
    return;
  }

  if (pendingFontAtlasRebuild_) {
    RebuildFontAtlas();
    pendingFontAtlasRebuild_ = false;
    return;
  }

  ImGui_ImplWin32_NewFrame();
  ImGui_ImplDX11_NewFrame();
  ImGui::NewFrame();

  DrawWindow();

  ImGui::Render();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
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
  if (!ImGui::Begin("Skyrim Outfit System NG", &open,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    if (!open) {
      Close();
    }
    return;
  }

  ImGui::TextUnformatted("Vanity outfit browser");
  ImGui::SameLine();
  ImGui::TextDisabled("| F3 toggles visibility");
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

  if (activeTab_ == BrowserTab::Options) {
    DrawOptionsTab();
  } else {
    workbench_.SyncRowsFromPlayer();

    if (ImGui::BeginTabBar("##catalog-tabs")) {
      if (ImGui::BeginTabItem("Gear")) {
        activeTab_ = BrowserTab::Gear;
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Outfits")) {
        activeTab_ = BrowserTab::Outfits;
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::Separator();
    DrawCatalogFilters();
    ImGui::Spacing();
    if (ImGui::Checkbox("Preview Selected", &previewSelected_)) {
      if (!previewSelected_) {
        workbench_.ClearPreview();
      } else if (selectedCatalogFormID_ != 0) {
        workbench_.ApplyCatalogPreview(selectedCatalogFormID_);
      }
    }
    ImGui::Spacing();

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
        if (activeTab_ == BrowserTab::Gear) {
          catalogRowClicked = DrawGearTab();
        } else {
          catalogRowClicked = DrawOutfitTab();
        }

        if (selectedCatalogFormID_ != 0 &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !catalogRowClicked) {
          ClearCatalogSelection();
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

    workbench_.SyncDynamicArmorVariants();
  }

  ImGui::End();

  if (!open) {
    Close();
  }
}

bool Menu::MatchesGearFilters(const GearEntry &a_entry) const {
  const auto &catalog = EquipmentCatalog::Get();
  if (gearPluginIndex_ > 0 &&
      a_entry.plugin != catalog.GetGearPlugins()[gearPluginIndex_ - 1]) {
    return false;
  }

  return MatchesSelectedSlotsOr(a_entry.slots) &&
         gearSearch_.PassFilter(a_entry.searchText.c_str());
}

bool Menu::MatchesOutfitFilters(const OutfitEntry &a_entry) const {
  const auto &catalog = EquipmentCatalog::Get();
  if (outfitPluginIndex_ > 0 &&
      a_entry.plugin != catalog.GetOutfitPlugins()[outfitPluginIndex_ - 1]) {
    return false;
  }

  return MatchesSelectedSlotsAnd(a_entry.slots) &&
         outfitSearch_.PassFilter(a_entry.searchText.c_str());
}

void Menu::SyncSelectedSlotFilters() {
  const auto size = EquipmentCatalog::Get().GetGearSlots().size();
  if (selectedSlotFilters_.size() != size) {
    selectedSlotFilters_.resize(size, false);
  }
}

bool Menu::HasAnySelectedSlotFilter() const {
  return std::ranges::any_of(selectedSlotFilters_,
                             [](bool a_selected) { return a_selected; });
}

bool Menu::MatchesSelectedSlotsOr(
    const std::vector<std::string> &a_slots) const {
  if (!HasAnySelectedSlotFilter()) {
    return true;
  }

  const auto &slotOptions = EquipmentCatalog::Get().GetGearSlots();
  for (std::size_t index = 0; index < selectedSlotFilters_.size(); ++index) {
    if (!selectedSlotFilters_[index]) {
      continue;
    }
    if (std::ranges::find(a_slots, slotOptions[index]) != a_slots.end()) {
      return true;
    }
  }

  return false;
}

bool Menu::MatchesSelectedSlotsAnd(
    const std::vector<std::string> &a_slots) const {
  if (!HasAnySelectedSlotFilter()) {
    return true;
  }

  const auto &slotOptions = EquipmentCatalog::Get().GetGearSlots();
  for (std::size_t index = 0; index < selectedSlotFilters_.size(); ++index) {
    if (!selectedSlotFilters_[index]) {
      continue;
    }
    if (std::ranges::find(a_slots, slotOptions[index]) == a_slots.end()) {
      return false;
    }
  }

  return true;
}

std::string Menu::BuildSelectedSlotPreview() const {
  const auto &slotOptions = EquipmentCatalog::Get().GetGearSlots();
  std::size_t selectedCount = 0;
  std::string_view selectedLabel = "Any slot";

  for (std::size_t index = 0; index < selectedSlotFilters_.size(); ++index) {
    if (!selectedSlotFilters_[index]) {
      continue;
    }
    ++selectedCount;
    selectedLabel = slotOptions[index];
  }

  if (selectedCount == 0) {
    return "Any slot";
  }
  if (selectedCount == 1) {
    return std::string(selectedLabel);
  }
  return std::to_string(selectedCount) + " slots";
}

std::vector<const GearEntry *> Menu::BuildFilteredGear() const {
  std::vector<const GearEntry *> rows;
  rows.reserve(EquipmentCatalog::Get().GetGear().size());
  for (const auto &entry : EquipmentCatalog::Get().GetGear()) {
    if (MatchesGearFilters(entry)) {
      rows.push_back(std::addressof(entry));
    }
  }
  return rows;
}

std::vector<const OutfitEntry *> Menu::BuildFilteredOutfits() const {
  std::vector<const OutfitEntry *> rows;
  rows.reserve(EquipmentCatalog::Get().GetOutfits().size());
  for (const auto &entry : EquipmentCatalog::Get().GetOutfits()) {
    if (MatchesOutfitFilters(entry)) {
      rows.push_back(std::addressof(entry));
    }
  }
  return rows;
}

void Menu::SortGearRows(std::vector<const GearEntry *> &a_rows,
                        ImGuiTableSortSpecs *a_sortSpecs) const {
  if (!a_sortSpecs || a_sortSpecs->SpecsCount == 0) {
    std::ranges::sort(a_rows, [](const auto *a_left, const auto *a_right) {
      return CompareText(a_left->name, a_right->name) < 0;
    });
    return;
  }

  const auto &spec = a_sortSpecs->Specs[0];
  const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

  std::ranges::sort(a_rows, [&](const auto *a_left, const auto *a_right) {
    int compare = 0;
    switch (static_cast<GearColumn>(spec.ColumnUserID)) {
    case GearColumn::Plugin:
      compare = CompareText(a_left->plugin, a_right->plugin);
      break;
    case GearColumn::Slot:
      compare = CompareText(a_left->slot, a_right->slot);
      if (compare == 0) {
        compare = CompareText(a_left->category, a_right->category);
      }
      break;
    case GearColumn::Name:
    default:
      compare = CompareText(a_left->name, a_right->name);
      break;
    }

    if (compare == 0) {
      compare = CompareText(a_left->name, a_right->name);
    }

    return ascending ? compare < 0 : compare > 0;
  });

  a_sortSpecs->SpecsDirty = false;
}

void Menu::SortOutfitRows(std::vector<const OutfitEntry *> &a_rows,
                          ImGuiTableSortSpecs *a_sortSpecs) const {
  if (!a_sortSpecs || a_sortSpecs->SpecsCount == 0) {
    std::ranges::sort(a_rows, [](const auto *a_left, const auto *a_right) {
      return CompareText(a_left->name, a_right->name) < 0;
    });
    return;
  }

  const auto &spec = a_sortSpecs->Specs[0];
  const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

  std::ranges::sort(a_rows, [&](const auto *a_left, const auto *a_right) {
    int compare = 0;
    switch (static_cast<OutfitColumn>(spec.ColumnUserID)) {
    case OutfitColumn::Plugin:
      compare = CompareText(a_left->plugin, a_right->plugin);
      break;
    case OutfitColumn::Pieces:
      compare = CompareText(a_left->piecesText, a_right->piecesText);
      break;
    case OutfitColumn::Name:
    default:
      compare = CompareText(a_left->name, a_right->name);
      break;
    }

    if (compare == 0) {
      compare = CompareText(a_left->name, a_right->name);
    }

    return ascending ? compare < 0 : compare > 0;
  });

  a_sortSpecs->SpecsDirty = false;
}

bool Menu::DrawSearchableStringCombo(const char *a_label,
                                     const char *a_allLabel,
                                     const std::vector<std::string> &a_options,
                                     int &a_index, ImGuiTextFilter &a_filter) {
  const char *preview =
      a_index == 0 ? a_allLabel
                   : a_options[static_cast<std::size_t>(a_index - 1)].c_str();
  bool changed = false;

  ImGui::PushID(a_label);
  if (ImGui::BeginCombo(a_label, preview)) {
    if (ImGui::IsWindowAppearing()) {
      a_filter.Clear();
    }

    a_filter.Draw("##search", -FLT_MIN);
    ImGui::Separator();

    if (a_filter.PassFilter(a_allLabel)) {
      const bool selected = a_index == 0;
      if (ImGui::Selectable(a_allLabel, selected)) {
        a_index = 0;
        changed = true;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    for (std::size_t index = 0; index < a_options.size(); ++index) {
      const auto &option = a_options[index];
      if (!a_filter.PassFilter(option.c_str())) {
        continue;
      }

      const bool selected = a_index == static_cast<int>(index + 1);
      if (ImGui::Selectable(option.c_str(), selected)) {
        a_index = static_cast<int>(index + 1);
        changed = true;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }
  ImGui::PopID();

  return changed;
}

void Menu::DrawCatalogFilters() {
  const auto &catalog = EquipmentCatalog::Get();
  SyncSelectedSlotFilters();
  const auto itemSpacingX = ImGui::GetStyle().ItemSpacing.x;
  const auto filterBarWidth = ImGui::GetContentRegionAvail().x;
  const auto drawSearchField = [&](ImGuiTextFilter &a_filter, float a_width,
                                   const char *a_id,
                                   const char *a_placeholder) {
    ImGui::PushItemWidth(a_width);
    a_filter.Draw(a_id, a_width);
    ImGui::PopItemWidth();
    if (a_filter.InputBuf[0] == '\0') {
      const auto rectMin = ImGui::GetItemRectMin();
      const auto padding = ImGui::GetStyle().FramePadding;
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(rectMin.x + padding.x, rectMin.y + padding.y),
          IM_COL32(140, 148, 161, 255), a_placeholder);
    }
  };
  const auto drawSlotMultiCombo = [&]() {
    const auto preview = BuildSelectedSlotPreview();
    if (ImGui::BeginCombo("##slot-filter", preview.c_str())) {
      const auto anySelected = !HasAnySelectedSlotFilter();
      if (ImGui::Selectable("Any slot", anySelected,
                            ImGuiSelectableFlags_DontClosePopups)) {
        std::fill(selectedSlotFilters_.begin(), selectedSlotFilters_.end(),
                  false);
      }
      if (anySelected) {
        ImGui::SetItemDefaultFocus();
      }

      ImGui::Separator();

      for (std::size_t index = 0; index < catalog.GetGearSlots().size();
           ++index) {
        bool selected = selectedSlotFilters_[index];
        if (ImGui::Selectable(catalog.GetGearSlots()[index].c_str(), selected,
                              ImGuiSelectableFlags_DontClosePopups)) {
          selectedSlotFilters_[index] = !selected;
        }
      }
      ImGui::EndCombo();
    }
  };

  if (filterBarWidth < 560.0f) {
    if (activeTab_ == BrowserTab::Gear) {
      drawSearchField(gearSearch_, -FLT_MIN, "##gear-search",
                      "Search installed armor");
    } else {
      drawSearchField(outfitSearch_, -FLT_MIN, "##outfit-search",
                      "Search installed outfits");
    }

    const auto halfWidth = (filterBarWidth - itemSpacingX) * 0.5f;
    ImGui::SetNextItemWidth(halfWidth);
    if (activeTab_ == BrowserTab::Gear) {
      DrawSearchableStringCombo("##gear-plugin", "All plugins",
                                catalog.GetGearPlugins(), gearPluginIndex_,
                                gearPluginFilter_);
    } else {
      DrawSearchableStringCombo("##outfit-plugin", "All plugins",
                                catalog.GetOutfitPlugins(), outfitPluginIndex_,
                                outfitPluginFilter_);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    drawSlotMultiCombo();
  } else {
    const auto searchWidth = filterBarWidth * 0.40f;
    const auto pluginWidth = filterBarWidth * 0.28f;
    const auto slotWidth =
        filterBarWidth - searchWidth - pluginWidth - (itemSpacingX * 2.0f);

    if (activeTab_ == BrowserTab::Gear) {
      drawSearchField(gearSearch_, searchWidth, "##gear-search",
                      "Search installed armor");
    } else {
      drawSearchField(outfitSearch_, searchWidth, "##outfit-search",
                      "Search installed outfits");
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(pluginWidth);
    if (activeTab_ == BrowserTab::Gear) {
      DrawSearchableStringCombo("##gear-plugin", "All plugins",
                                catalog.GetGearPlugins(), gearPluginIndex_,
                                gearPluginFilter_);
    } else {
      DrawSearchableStringCombo("##outfit-plugin", "All plugins",
                                catalog.GetOutfitPlugins(), outfitPluginIndex_,
                                outfitPluginFilter_);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(slotWidth);
    drawSlotMultiCombo();
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
  if (ImGui::BeginTable("##gear-table", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 0.0f))) {
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Name));
    ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Plugin));
    ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Slot));
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
        workbench::EquipmentWidgetItem item{};
        if (!workbench_.BuildCatalogItem(entry.formID, item)) {
          item.formID = entry.formID;
          item.key = "catalog:" + entry.id;
          item.name = entry.name;
          item.slotText = entry.slot;
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight =
            18.0f + (ImGui::GetTextLineHeight() * 2.0f) + 14.0f;
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected =
            selectedCatalogFormID_ == item.formID &&
            (!previewSelected_ || workbench_.IsPreviewingForm(item.formID));
        const bool clicked = ImGui::Selectable(
            ("##catalog-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        ImGui::SetCursorScreenPos(rowContentPos);
        DrawEquipmentInfoWidget(item.key.c_str(), item, true,
                                DragSourceKind::Catalog);

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(entry.plugin.data());

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", entry.slot.data());

        if (selected) {
          ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                 IM_COL32(54, 84, 118, 132));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                 IM_COL32(44, 58, 73, 112));
        }

        if (clicked) {
          rowClicked = true;
          selectedCatalogFormID_ = item.formID;
          if (previewSelected_) {
            workbench_.ApplyCatalogPreview(item.formID);
          } else {
            workbench_.ClearPreview();
          }
        }

        if (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          rowClicked = true;
          workbench_.AddCatalogSelectionToWorkbench(item.formID);
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

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected =
            selectedCatalogFormID_ == outfit.formID &&
            (!previewSelected_ || workbench_.IsPreviewingForm(outfit.formID));
        const bool clicked = ImGui::Selectable(
            ("##outfit-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        ImGui::SetCursorScreenPos(rowContentPos);

        if (selected) {
          ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                 IM_COL32(54, 84, 118, 132));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                 IM_COL32(44, 58, 73, 112));
        }

        if (clicked) {
          rowClicked = true;
          selectedCatalogFormID_ = outfit.formID;
          if (previewSelected_) {
            workbench_.ApplyCatalogPreview(outfit.formID);
          } else {
            workbench_.ClearPreview();
          }
        }

        if (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          rowClicked = true;
          workbench_.AddCatalogSelectionToWorkbench(outfit.formID);
        }

        ImGui::TextUnformatted(outfit.name.data());

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(outfit.plugin.data());

        ImGui::TableSetColumnIndex(2);
        {
          const auto availableWidth = ImGui::GetContentRegionAvail().x;
          const auto displayText =
              TruncateTextToWidth(outfit.piecesText, availableWidth);
          ImGui::TextUnformatted(displayText.c_str());
          if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) &&
              displayText != outfit.piecesText) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(outfit.piecesText.c_str());
            ImGui::EndTooltip();
          }
        }
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}

void Menu::DrawOptionsTab() {
  ClearCatalogSelection();
  ImGui::TextUnformatted("Interface");
  ImGui::Separator();
  ImGui::TextWrapped("Adjust the UI font size for this session. The value is "
                     "stored in integer pixels and the font atlas rebuild is "
                     "applied after you finish editing.");

  ImGui::SliderInt("Font size", &pendingFontSizePixels_, kMinFontSizePixels,
                   kMaxFontSizePixels, "%d px");
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    fontSizePixels_ = pendingFontSizePixels_;
    SaveUserSettings();
    pendingFontAtlasRebuild_ = true;
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset")) {
    fontSizePixels_ = kDefaultFontSizePixels;
    pendingFontSizePixels_ = fontSizePixels_;
    SaveUserSettings();
    pendingFontAtlasRebuild_ = true;
  }
}
} // namespace sosng
