#include "Menu.h"

#include "ArmorUtils.h"
#include "InputManager.h"
#include "MenuHost.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui_internal.h"
#include "ui/components/EditableCombo.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_set>

namespace {
constexpr auto kSettingsDirectory =
    "Data/SKSE/Plugins/SkyrimOutfitSystemRedone";
constexpr auto kImGuiIniFilename = "imgui.ini";
constexpr auto kUserSettingsFilename = "settings.json";
constexpr auto kFavoritesFilename = "favorites.json";
constexpr auto kModexKitDirectory = "data/interface/modex/user/kits";
constexpr auto kDefaultFontPath =
    "Data/Interface/SkyrimOutfitSystemRedone/fonts/Ubuntu-R.ttf";
constexpr auto kDefaultIconFontPath =
    "Data/Interface/SkyrimOutfitSystemRedone/fonts/lucide.ttf";
constexpr int kDefaultFontSizePixels = 16;
constexpr int kMinFontSizePixels = 8;
constexpr int kMaxFontSizePixels = 48;
constexpr float kFadeDurationSeconds = 0.30f;
constexpr float kSmoothScrollStepMultiplier = 5.0f;
constexpr float kSmoothScrollLerpFactor = 0.20f;

using UserEventFlag = RE::ControlMap::UEFlag;

constexpr auto kBlockedGameplayControls = static_cast<UserEventFlag>(
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kMovement) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kLooking) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kActivate) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kFighting) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kSneaking) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kJumping) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kPOVSwitch) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kMainFour) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kWheelZoom) |
    static_cast<std::underlying_type_t<UserEventFlag>>(UserEventFlag::kVATS));

enum class GearColumn : ImGuiID { Name = 1, Plugin };

enum class OutfitColumn : ImGuiID { Name = 1, Plugin, Pieces };

enum class KitColumn : ImGuiID { Name = 1, Collection, Pieces };

constexpr std::string_view kFavoritePrefix = "\xEE\x83\xB5 ";
constexpr std::array<ImWchar, 3> kLucideIconRanges = {0xE038, 0xE63F, 0};

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

std::vector<RE::FormID> CollectOutfitArmorFormIDs(const RE::FormID a_formID) {
  std::vector<RE::FormID> armorFormIDs;
  std::unordered_set<RE::FormID> seenArmorForms;

  const auto *outfit = RE::TESForm::LookupByID<RE::BGSOutfit>(a_formID);
  if (!outfit) {
    return armorFormIDs;
  }

  outfit->ForEachItem([&](RE::TESForm *a_item) {
    const auto *armor = a_item ? a_item->As<RE::TESObjectARMO>() : nullptr;
    if (armor && seenArmorForms.insert(armor->GetFormID()).second) {
      armorFormIDs.push_back(armor->GetFormID());
    }
    return RE::BSContainer::ForEachResult::kContinue;
  });

  return armorFormIDs;
}

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

void AllowTextInput(RE::ControlMap *a_controlMap, bool a_allow) {
  using Func = decltype(&AllowTextInput);
  static REL::Relocation<Func> func{RELOCATION_ID(67252, 68552)};
  func(a_controlMap, a_allow);
}

} // namespace

namespace sosr {
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
  favoritesPath_ =
      (std::filesystem::path(settingsDirectory_) / kFavoritesFilename).string();
  LoadUserSettings();
  LoadFavorites();

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

  ThemeConfig::GetSingleton()->Load(themeName_);
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
    pauseGameWhenOpen_ = json.value("pauseGameWhileOpen", pauseGameWhenOpen_);
    smoothScroll_ = json.value("smoothScroll", smoothScroll_);
    toggleKey_ = json.value("toggleKey", toggleKey_);
    toggleModifier_ = json.value("toggleModifier", toggleModifier_);
    themeName_ = json.value("theme", themeName_);
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

  const nlohmann::json json = {{"fontSizePx", fontSizePixels_},
                               {"pauseGameWhileOpen", pauseGameWhenOpen_},
                               {"smoothScroll", smoothScroll_},
                               {"toggleKey", toggleKey_},
                               {"toggleModifier", toggleModifier_},
                               {"theme", themeName_}};

  output << json.dump(2) << '\n';
}

void Menu::LoadFavorites() {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create favorites directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  favoriteKeys_.clear();

  std::ifstream input(favoritesPath_);
  if (!input.is_open()) {
    SaveFavorites();
    return;
  }

  try {
    const auto json = nlohmann::json::parse(input, nullptr, true, true);
    if (const auto it = json.find("favorites");
        it != json.end() && it->is_array()) {
      for (const auto &entry : *it) {
        if (entry.is_string()) {
          favoriteKeys_.insert(entry.get<std::string>());
        }
      }
    }
  } catch (const std::exception &exception) {
    logger::warn("Failed to parse favorites from {}: {}", favoritesPath_,
                 exception.what());
  }
}

void Menu::SaveFavorites() const {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create favorites directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  std::ofstream output(favoritesPath_, std::ios::trunc);
  if (!output.is_open()) {
    logger::error("Failed to write favorites to {}", favoritesPath_);
    return;
  }

  std::vector<std::string> favorites(favoriteKeys_.begin(),
                                     favoriteKeys_.end());
  std::ranges::sort(favorites);
  const nlohmann::json json = {{"favorites", favorites}};
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
  if (io.FontDefault && std::filesystem::exists(kDefaultIconFontPath)) {
    ImFontConfig iconConfig{};
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphOffset.y = 3.0f;
    iconConfig.DstFont = io.FontDefault;
    io.Fonts->AddFontFromFileTTF(kDefaultIconFontPath,
                                 static_cast<float>(fontSizePixels_),
                                 &iconConfig, kLucideIconRanges.data());
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
  style.WindowRounding = 4.0f;
  style.ChildRounding = 3.0f;
  style.FrameRounding = 4.0f;
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 4.0f;
  style.FrameBorderSize = 1.0f;
  style.WindowBorderSize = 1.0f;
  style.WindowPadding = {12.0f, 12.0f};
  style.FramePadding = {8.0f, 6.0f};
  style.ItemSpacing = {8.0f, 6.0f};
  style.CellPadding = {6.0f, 5.0f};
  ThemeConfig::GetSingleton()->ApplyToImGui();
}

void Menu::Open() {
  if (!initialized_ || enabled_ || !gameDataLoaded_) {
    return;
  }

  if (auto *messageQueue = RE::UIMessageQueue::GetSingleton();
      messageQueue != nullptr) {
    messageQueue->AddMessage(MenuHost::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow,
                             nullptr);
  }
}

void Menu::Close() {
  if (!initialized_ || !enabled_ ||
      visibilityState_ == VisibilityState::Closing) {
    return;
  }
  visibilityState_ = VisibilityState::Closing;
}

void Menu::OnMenuShow() {
  if (!initialized_ || enabled_) {
    return;
  }

  if (auto *controlMap = RE::ControlMap::GetSingleton();
      controlMap != nullptr) {
    controlMap->ToggleControls(kBlockedGameplayControls, false, false);
  }

  auto &io = ImGui::GetIO();
  io.MouseDrawCursor = false;
  io.ClearInputKeys();
  io.ClearEventsQueue();
  visibilityState_ = VisibilityState::Opening;
  windowAlpha_ = 0.0f;
  hideMessageQueued_ = false;
  pendingSmoothWheelDelta_ = 0.0f;
  smoothScrollWindowId_ = 0;
  smoothScrollTargetY_ = 0.0f;
  enabled_ = true;
}

void Menu::OnMenuHide() {
  if (!initialized_ || !enabled_) {
    return;
  }

  ClearCatalogSelection();
  CloseToggleKeyCapture();

  if (auto *controlMap = RE::ControlMap::GetSingleton();
      controlMap != nullptr) {
    if (wantTextInput_) {
      AllowTextInput(controlMap, false);
      wantTextInput_ = false;
    }
    controlMap->ToggleControls(kBlockedGameplayControls, true, false);
  }

  auto &io = ImGui::GetIO();
  io.MouseDrawCursor = false;
  io.ClearInputKeys();
  io.ClearEventsQueue();
  pendingSmoothWheelDelta_ = 0.0f;
  smoothScrollWindowId_ = 0;
  smoothScrollTargetY_ = 0.0f;
  hideMessageQueued_ = false;
  visibilityState_ = VisibilityState::Closed;
  windowAlpha_ = 0.0f;
  if (io.IniFilename) {
    ImGui::SaveIniSettingsToDisk(io.IniFilename);
  }
  SaveUserSettings();
  enabled_ = false;
}

void Menu::HandleCancel() {
  if (createKitDialogOpen_) {
    createKitDialogCancelRequested_ = true;
    return;
  }

  if (awaitingToggleKeyCapture_ || openToggleKeyPopup_) {
    CloseToggleKeyCapture();
    return;
  }

  Close();
}

void Menu::Toggle() {
  if (enabled_) {
    Close();
  } else {
    Open();
  }
}

bool Menu::QueueSmoothScroll(const float a_deltaY) {
  if (!enabled_ || !smoothScroll_ || a_deltaY == 0.0f) {
    return false;
  }

  pendingSmoothWheelDelta_ += a_deltaY;
  return true;
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

void Menu::AddGearEntryToWorkbench(const GearEntry &a_entry) {
  workbench_.AddCatalogSelectionToWorkbench(
      std::vector<RE::FormID>{a_entry.formID});
}

void Menu::AddOutfitEntryToWorkbench(const OutfitEntry &a_entry) {
  workbench_.AddCatalogSelectionToWorkbench(a_entry.formID);
}

void Menu::AddKitEntryToWorkbench(const KitEntry &a_entry) {
  workbench_.AddCatalogSelectionToWorkbench(a_entry.armorFormIDs);
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
        return CompareText(a_kit.name, name) == 0;
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
  data[name]["Description"] = "Created by Skyrim Outfit System Redone.";
  data[name]["Items"] = std::move(items);

  std::ofstream file(fullPath, std::ios::trunc);
  if (!file.is_open()) {
    createKitError_ = "Failed to open kit file for writing.";
    return false;
  }

  file << data.dump(4) << '\n';
  file.close();

  EquipmentCatalog::Get().RefreshFromGame();
  selectedCatalogKey_ = "kit:" + relativePath.generic_string();
  activeTab_ = BrowserTab::Kits;
  openCreateKitDialog_ = false;
  pendingKitFormIDs_.clear();
  createKitError_.clear();
  return true;
}

void Menu::PreviewGearEntry(const GearEntry &a_entry) {
  workbench_.ApplyCatalogPreview(a_entry.id,
                                 std::vector<RE::FormID>{a_entry.formID});
}

void Menu::PreviewOutfitEntry(const OutfitEntry &a_entry) {
  workbench_.ApplyCatalogPreview(a_entry.id,
                                 CollectOutfitArmorFormIDs(a_entry.formID));
}

void Menu::PreviewKitEntry(const KitEntry &a_entry) {
  workbench_.ApplyCatalogPreview(a_entry.id, a_entry.armorFormIDs);
}

void Menu::QueueHideMessage() {
  if (hideMessageQueued_) {
    return;
  }

  if (auto *messageQueue = RE::UIMessageQueue::GetSingleton();
      messageQueue != nullptr) {
    messageQueue->AddMessage(MenuHost::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide,
                             nullptr);
    hideMessageQueued_ = true;
  }
}

void Menu::UpdateVisibilityAnimation(const float a_deltaTime) {
  switch (visibilityState_) {
  case VisibilityState::Opening:
    windowAlpha_ += a_deltaTime / kFadeDurationSeconds;
    if (windowAlpha_ >= 1.0f) {
      windowAlpha_ = 1.0f;
      visibilityState_ = VisibilityState::Open;
    }
    break;
  case VisibilityState::Closing:
    windowAlpha_ -= a_deltaTime / kFadeDurationSeconds;
    if (windowAlpha_ <= 0.0f) {
      windowAlpha_ = 0.0f;
      QueueHideMessage();
    }
    break;
  case VisibilityState::Open:
    windowAlpha_ = 1.0f;
    break;
  case VisibilityState::Closed:
    windowAlpha_ = 0.0f;
    break;
  }
}

void Menu::ApplySmoothScroll() {
  if (!smoothScroll_) {
    pendingSmoothWheelDelta_ = 0.0f;
    smoothScrollWindowId_ = 0;
    return;
  }

  auto *context = ImGui::GetCurrentContext();
  if (context == nullptr) {
    pendingSmoothWheelDelta_ = 0.0f;
    smoothScrollWindowId_ = 0;
    return;
  }

  auto findScrollWindow = [](ImGuiWindow *a_window) -> ImGuiWindow * {
    while (a_window != nullptr) {
      if (!(a_window->Flags & ImGuiWindowFlags_NoScrollWithMouse) &&
          a_window->ScrollMax.y > 0.0f) {
        return a_window;
      }
      a_window = a_window->ParentWindow;
    }
    return nullptr;
  };

  if (pendingSmoothWheelDelta_ != 0.0f) {
    if (auto *scrollWindow = findScrollWindow(context->HoveredWindow);
        scrollWindow != nullptr) {
      if (smoothScrollWindowId_ != scrollWindow->ID) {
        smoothScrollWindowId_ = scrollWindow->ID;
        smoothScrollTargetY_ = scrollWindow->Scroll.y;
      }

      const auto scrollStep =
          ImGui::GetTextLineHeightWithSpacing() * kSmoothScrollStepMultiplier;
      smoothScrollTargetY_ = std::clamp(
          smoothScrollTargetY_ - pendingSmoothWheelDelta_ * scrollStep, 0.0f,
          scrollWindow->ScrollMax.y);
    }
    pendingSmoothWheelDelta_ = 0.0f;
  }

  if (smoothScrollWindowId_ == 0) {
    return;
  }

  ImGuiWindow *scrollWindow = nullptr;
  for (auto *window : context->Windows) {
    if (window->ID == smoothScrollWindowId_) {
      scrollWindow = window;
      break;
    }
  }

  if (scrollWindow == nullptr) {
    smoothScrollWindowId_ = 0;
    smoothScrollTargetY_ = 0.0f;
    return;
  }

  smoothScrollTargetY_ =
      std::clamp(smoothScrollTargetY_, 0.0f, scrollWindow->ScrollMax.y);
  const auto currentScrollY = scrollWindow->Scroll.y;
  auto nextScrollY = currentScrollY + ((smoothScrollTargetY_ - currentScrollY) *
                                       kSmoothScrollLerpFactor);
  if (std::abs(smoothScrollTargetY_ - nextScrollY) <= 0.5f) {
    nextScrollY = smoothScrollTargetY_;
  }

  scrollWindow->Scroll.y = nextScrollY;
  scrollWindow->ScrollTarget.y = nextScrollY;
  scrollWindow->ScrollTargetCenterRatio.y = 0.0f;

  if (std::abs(smoothScrollTargetY_ - nextScrollY) <= 0.5f) {
    smoothScrollWindowId_ = 0;
  }
}

void Menu::OpenToggleKeyCapture() {
  awaitingToggleKeyCapture_ = true;
  openToggleKeyPopup_ = true;
  toggleKeyCaptureError_.clear();
}

void Menu::SyncAllowTextInput() {
  const bool currentWantTextInput = ImGui::GetIO().WantTextInput;

  if (!wantTextInput_ && currentWantTextInput) {
    if (auto *controlMap = RE::ControlMap::GetSingleton();
        controlMap != nullptr) {
      AllowTextInput(controlMap, true);
    }
  } else if (wantTextInput_ && !currentWantTextInput) {
    if (auto *controlMap = RE::ControlMap::GetSingleton();
        controlMap != nullptr) {
      AllowTextInput(controlMap, false);
    }
  }

  wantTextInput_ = currentWantTextInput;
}

void Menu::CloseToggleKeyCapture() {
  awaitingToggleKeyCapture_ = false;
  openToggleKeyPopup_ = false;
  toggleKeyCaptureError_.clear();
}

void Menu::HandleToggleKeyCapture(const std::uint32_t a_scanCode,
                                  const std::uint32_t a_modifierScanCode) {
  if (a_scanCode == 0x01) {
    CloseToggleKeyCapture();
    return;
  }

  if (a_scanCode == 0x14) {
    toggleKey_ = 0x40;
    toggleModifier_ = 0;
    SaveUserSettings();
    CloseToggleKeyCapture();
    return;
  }

  if (!keycode::IsValidHotkey(a_scanCode) ||
      keycode::IsKeyModifier(a_scanCode)) {
    toggleKeyCaptureError_ =
        "Choose a non-modifier key. Hold Shift, Ctrl, or Alt for a modifier.";
    return;
  }

  toggleKey_ = a_scanCode;
  toggleModifier_ = a_modifierScanCode;
  SaveUserSettings();
  CloseToggleKeyCapture();
}

std::string Menu::GetToggleKeyLabel() const {
  if (toggleModifier_ != 0) {
    return keycode::GetKeyName(toggleModifier_) + " + " +
           keycode::GetKeyName(toggleKey_);
  }

  return keycode::GetKeyName(toggleKey_);
}

void Menu::Draw() {
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
  InputManager::GetSingleton()->UpdateMousePosition();
  ImGui::NewFrame();
  SyncAllowTextInput();

  DrawWindow();
  ApplySmoothScroll();
  UpdateVisibilityAnimation(ImGui::GetIO().DeltaTime);

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
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, windowAlpha_);
  if (!ImGui::Begin("Skyrim Outfit System Redone", &open,
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

  if (activeTab_ == BrowserTab::Options) {
    DrawOptionsTab();
  } else {
    workbench_.SyncRowsFromPlayer();
    const auto applySelectedPreview = [&]() {
      if (!previewSelected_ || selectedCatalogKey_.empty()) {
        return;
      }

      if (activeTab_ == BrowserTab::Gear) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetGear(), selectedCatalogKey_,
            [](const GearEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetGear().end()) {
          workbench_.ApplyCatalogPreview(
              entry->id, std::vector<RE::FormID>{entry->formID});
        }
      } else if (activeTab_ == BrowserTab::Outfits) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetOutfits(), selectedCatalogKey_,
            [](const OutfitEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetOutfits().end()) {
          workbench_.ApplyCatalogPreview(
              entry->id, CollectOutfitArmorFormIDs(entry->formID));
        }
      } else if (activeTab_ == BrowserTab::Kits) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetKits(), selectedCatalogKey_,
            [](const KitEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetKits().end()) {
          workbench_.ApplyCatalogPreview(entry->id, entry->armorFormIDs);
        }
      }
    };

    if (ImGui::BeginTabBar("##catalog-tabs")) {
      if (ImGui::BeginTabItem("Gear")) {
        if (activeTab_ != BrowserTab::Gear) {
          ClearCatalogSelection();
        }
        activeTab_ = BrowserTab::Gear;
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Outfits")) {
        if (activeTab_ != BrowserTab::Outfits) {
          ClearCatalogSelection();
        }
        activeTab_ = BrowserTab::Outfits;
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Kits")) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
          ImGui::BeginTooltip();
          ImGui::TextUnformatted("These are Modex kits loaded from "
                                 "data/interface/modex/user/kits.");
          ImGui::TextUnformatted(
              "Kits that refer to non-existent items are not shown.");
          ImGui::EndTooltip();
        }
        if (activeTab_ != BrowserTab::Kits) {
          ClearCatalogSelection();
        }
        activeTab_ = BrowserTab::Kits;
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::Separator();
    DrawCatalogFilters();
    ImGui::Spacing();
    if (ImGui::Checkbox("Favorites Only", &favoritesOnly_) && favoritesOnly_ &&
        !selectedCatalogKey_.empty() &&
        !IsFavorite(activeTab_, selectedCatalogKey_)) {
      ClearCatalogSelection();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Preview Selected", &previewSelected_)) {
      if (!previewSelected_) {
        workbench_.ClearPreview();
      } else {
        applySelectedPreview();
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
        } else if (activeTab_ == BrowserTab::Outfits) {
          catalogRowClicked = DrawOutfitTab();
        } else {
          catalogRowClicked = DrawKitTab();
        }

        if (!selectedCatalogKey_.empty() &&
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

    workbench_.SyncDynamicArmorVariantsExtended();
  }

  DrawCreateKitDialog();

  ImGui::End();
  ImGui::PopStyleVar();

  if (!open) {
    Close();
  }
}

bool Menu::MatchesGearFilters(const GearEntry &a_entry) const {
  if (favoritesOnly_ && !IsFavorite(BrowserTab::Gear, a_entry.id)) {
    return false;
  }

  const auto &catalog = EquipmentCatalog::Get();
  if (gearPluginIndex_ > 0 &&
      a_entry.plugin != catalog.GetGearPlugins()[gearPluginIndex_ - 1]) {
    return false;
  }

  return MatchesSelectedSlotsOr(a_entry.slots) &&
         gearSearch_.PassFilter(a_entry.searchText.c_str());
}

bool Menu::MatchesOutfitFilters(const OutfitEntry &a_entry) const {
  if (favoritesOnly_ && !IsFavorite(BrowserTab::Outfits, a_entry.id)) {
    return false;
  }

  const auto &catalog = EquipmentCatalog::Get();
  if (outfitPluginIndex_ > 0 &&
      a_entry.plugin != catalog.GetOutfitPlugins()[outfitPluginIndex_ - 1]) {
    return false;
  }

  return MatchesSelectedSlotsAnd(a_entry.slots) &&
         outfitSearch_.PassFilter(a_entry.searchText.c_str());
}

bool Menu::MatchesKitFilters(const KitEntry &a_entry) const {
  if (favoritesOnly_ && !IsFavorite(BrowserTab::Kits, a_entry.id)) {
    return false;
  }

  const auto &catalog = EquipmentCatalog::Get();
  if (kitCollectionIndex_ > 0 &&
      a_entry.collection !=
          catalog.GetKitCollections()[kitCollectionIndex_ - 1]) {
    return false;
  }

  return MatchesSelectedSlotsAnd(a_entry.slots) &&
         kitSearch_.PassFilter(a_entry.searchText.c_str());
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

std::vector<const KitEntry *> Menu::BuildFilteredKits() const {
  std::vector<const KitEntry *> rows;
  rows.reserve(EquipmentCatalog::Get().GetKits().size());
  for (const auto &entry : EquipmentCatalog::Get().GetKits()) {
    if (MatchesKitFilters(entry)) {
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

void Menu::SortKitRows(std::vector<const KitEntry *> &a_rows,
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
    switch (static_cast<KitColumn>(spec.ColumnUserID)) {
    case KitColumn::Collection:
      compare = CompareText(a_left->collection, a_right->collection);
      break;
    case KitColumn::Pieces:
      compare = CompareText(a_left->piecesText, a_right->piecesText);
      break;
    case KitColumn::Name:
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

void Menu::DrawCatalogFilters() {
  const auto &catalog = EquipmentCatalog::Get();
  SyncSelectedSlotFilters();
  const auto itemSpacingX = ImGui::GetStyle().ItemSpacing.x;
  const auto filterBarWidth = ImGui::GetContentRegionAvail().x;
  const auto drawSearchField = [&](ImGuiTextFilter &a_filter, float a_width,
                                   const char *a_id,
                                   const char *a_placeholder) {
    ImGui::PushItemWidth(a_width);
    if (ImGui::InputText(a_id, a_filter.InputBuf,
                         IM_ARRAYSIZE(a_filter.InputBuf),
                         ImGuiInputTextFlags_AutoSelectAll)) {
      a_filter.Build();
    }
    ImGui::PopItemWidth();
    const auto rectMin = ImGui::GetItemRectMin();
    const auto rectMax = ImGui::GetItemRectMax();
    ui::components::DrawTextInputOutline(
        rectMin, rectMax, ImGui::IsItemHovered(), ImGui::IsItemActive());
    if (ImGui::IsItemActivated()) {
      InputManager::GetSingleton()->Flush();
    }
    if (a_filter.InputBuf[0] == '\0') {
      const auto padding = ImGui::GetStyle().FramePadding;
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(rectMin.x + padding.x, rectMin.y + padding.y),
          ThemeConfig::GetSingleton()->GetColorU32("TEXT_DISABLED", 0.90f),
          a_placeholder);
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
    } else if (activeTab_ == BrowserTab::Outfits) {
      drawSearchField(outfitSearch_, -FLT_MIN, "##outfit-search",
                      "Search installed outfits");
    } else {
      drawSearchField(kitSearch_, -FLT_MIN, "##kit-search",
                      "Search Modex kits");
    }

    const auto halfWidth = (filterBarWidth - itemSpacingX) * 0.5f;
    ImGui::SetNextItemWidth(halfWidth);
    if (activeTab_ == BrowserTab::Gear) {
      ui::components::DrawSearchableStringCombo(
          "##gear-plugin", "All plugins", catalog.GetGearPlugins(),
          gearPluginIndex_, gearPluginFilter_);
    } else if (activeTab_ == BrowserTab::Outfits) {
      ui::components::DrawSearchableStringCombo(
          "##outfit-plugin", "All plugins", catalog.GetOutfitPlugins(),
          outfitPluginIndex_, outfitPluginFilter_);
    } else {
      ui::components::DrawSearchableStringCombo(
          "##kit-collection", "All collections", catalog.GetKitCollections(),
          kitCollectionIndex_, kitCollectionFilter_);
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
    } else if (activeTab_ == BrowserTab::Outfits) {
      drawSearchField(outfitSearch_, searchWidth, "##outfit-search",
                      "Search installed outfits");
    } else {
      drawSearchField(kitSearch_, searchWidth, "##kit-search",
                      "Search Modex kits");
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(pluginWidth);
    if (activeTab_ == BrowserTab::Gear) {
      ui::components::DrawSearchableStringCombo(
          "##gear-plugin", "All plugins", catalog.GetGearPlugins(),
          gearPluginIndex_, gearPluginFilter_);
    } else if (activeTab_ == BrowserTab::Outfits) {
      ui::components::DrawSearchableStringCombo(
          "##outfit-plugin", "All plugins", catalog.GetOutfitPlugins(),
          outfitPluginIndex_, outfitPluginFilter_);
    } else {
      ui::components::DrawSearchableStringCombo(
          "##kit-collection", "All collections", catalog.GetKitCollections(),
          kitCollectionIndex_, kitCollectionFilter_);
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
        const auto rowHeight =
            18.0f + (ImGui::GetTextLineHeight() * 2.0f) + 14.0f;
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
          if (ImGui::MenuItem("Add Override")) {
            AddGearEntryToWorkbench(entry);
          }
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);
        DrawEquipmentInfoWidget(item.key.c_str(), item, true,
                                DragSourceKind::Catalog);

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

        if (clicked) {
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

        if (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
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
          if (ImGui::MenuItem("Add Override")) {
            AddOutfitEntryToWorkbench(outfit);
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
          AddOutfitEntryToWorkbench(outfit);
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
          if (ImGui::MenuItem("Add Override")) {
            AddKitEntryToWorkbench(kit);
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
          AddKitEntryToWorkbench(kit);
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
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) &&
            displayText != kit.piecesText) {
          ImGui::BeginTooltip();
          ImGui::TextUnformatted(kit.piecesText.c_str());
          ImGui::EndTooltip();
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

  if (ImGui::BeginChild("##options-font-panel", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_Borders |
                            ImGuiChildFlags_AutoResizeY)) {
    if (ImGui::BeginTable("##options-layout", 2,
                          ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0.0f, 0.0f))) {
      ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch,
                              1.15f);
      ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch,
                              0.85f);
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Font Size");
      ImGui::TextDisabled("Range: %d px to %d px", kMinFontSizePixels,
                          kMaxFontSizePixels);

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::SliderInt("##font-size", &pendingFontSizePixels_,
                       kMinFontSizePixels, kMaxFontSizePixels, "%d px");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        fontSizePixels_ = pendingFontSizePixels_;
        SaveUserSettings();
        pendingFontAtlasRebuild_ = true;
      }

      if (ImGui::Button("Reset to Default")) {
        fontSizePixels_ = kDefaultFontSizePixels;
        pendingFontSizePixels_ = fontSizePixels_;
        SaveUserSettings();
        pendingFontAtlasRebuild_ = true;
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Toggle UI Button");

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::Button(GetToggleKeyLabel().c_str(), ImVec2(-FLT_MIN, 0.0f))) {
        OpenToggleKeyCapture();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Pause Game While Open");

      ImGui::TableSetColumnIndex(1);
      bool pauseGameWhenOpen = pauseGameWhenOpen_;
      if (ImGui::Checkbox("##pause-game-while-open", &pauseGameWhenOpen)) {
        pauseGameWhenOpen_ = pauseGameWhenOpen;
        SaveUserSettings();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Smooth Scrolling");

      ImGui::TableSetColumnIndex(1);
      bool smoothScroll = smoothScroll_;
      if (ImGui::Checkbox("##smooth-scrolling", &smoothScroll)) {
        smoothScroll_ = smoothScroll;
        pendingSmoothWheelDelta_ = 0.0f;
        smoothScrollWindowId_ = 0;
        smoothScrollTargetY_ = 0.0f;
        SaveUserSettings();
      }

      ImGui::EndTable();
    }
    ImGui::EndChild();
  }

  if (openToggleKeyPopup_) {
    ImGui::OpenPopup("##toggle-key-capture");
    openToggleKeyPopup_ = false;
  }

  ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("##toggle-key-capture", nullptr,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove)) {
    ImGui::TextUnformatted("Change Toggle UI Button");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Press the key combination you want to use. Hold Shift, Ctrl, or Alt "
        "while pressing the key to include a modifier.");
    ImGui::TextWrapped(
        "Press T to reset to default (F6). Press Escape to cancel.");
    if (!toggleKeyCaptureError_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(ThemeConfig::GetSingleton()->GetColor("ERROR"), "%s",
                         toggleKeyCaptureError_.c_str());
    }
    ImGui::Spacing();
    if (ImGui::Button("Cancel", ImVec2(-FLT_MIN, 0.0f))) {
      CloseToggleKeyCapture();
      ImGui::CloseCurrentPopup();
    }
    if (!awaitingToggleKeyCapture_) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
} // namespace sosr
