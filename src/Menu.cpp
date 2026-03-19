#include "Menu.h"

#include "InputManager.h"
#include "integrations/DynamicArmorVariantsClient.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui_internal.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

namespace
{
    using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;

    constexpr auto kSettingsDirectory = "Data/SKSE/Plugins/SkyrimOutfitSystemNG";
    constexpr auto kImGuiIniFilename = "imgui.ini";
    constexpr auto kUserSettingsFilename = "settings.json";
    constexpr auto kDefaultFontPath = "Data/Interface/SkyrimOutfitSystemNG/fonts/Ubuntu-R.ttf";
    constexpr int kDefaultFontSizePixels = 16;
    constexpr int kMinFontSizePixels = 8;
    constexpr int kMaxFontSizePixels = 28;
    constexpr char kVariantItemPayloadType[] = "SOSNG_VARIANT_ITEM";

    constexpr std::array<std::pair<BipedSlot, std::string_view>, 32> kArmorSlotNames{ {
        { BipedSlot::kHead, "30 - Head" },
        { BipedSlot::kHair, "31 - Hair" },
        { BipedSlot::kBody, "32 - Body" },
        { BipedSlot::kHands, "33 - Hands" },
        { BipedSlot::kForearms, "34 - Forearms" },
        { BipedSlot::kAmulet, "35 - Amulet" },
        { BipedSlot::kRing, "36 - Ring" },
        { BipedSlot::kFeet, "37 - Feet" },
        { BipedSlot::kCalves, "38 - Calves" },
        { BipedSlot::kShield, "39 - Shield" },
        { BipedSlot::kTail, "40 - Tail" },
        { BipedSlot::kLongHair, "41 - Long Hair" },
        { BipedSlot::kCirclet, "42 - Circlet" },
        { BipedSlot::kEars, "43 - Ears" },
        { BipedSlot::kModMouth, "44 - Mod Mouth" },
        { BipedSlot::kModNeck, "45 - Mod Neck" },
        { BipedSlot::kModChestPrimary, "46 - Mod Chest Primary" },
        { BipedSlot::kModBack, "47 - Mod Back" },
        { BipedSlot::kModMisc1, "48 - Mod Misc1" },
        { BipedSlot::kModPelvisPrimary, "49 - Mod Pelvis Primary" },
        { BipedSlot::kDecapitateHead, "50 - Decapitate Head" },
        { BipedSlot::kDecapitate, "51 - Decapitate" },
        { BipedSlot::kModPelvisSecondary, "52 - Mod Pelvis Secondary" },
        { BipedSlot::kModLegRight, "53 - Mod Leg Right" },
        { BipedSlot::kModLegLeft, "54 - Mod Leg Left" },
        { BipedSlot::kModFaceJewelry, "55 - Mod Face Jewelry" },
        { BipedSlot::kModChestSecondary, "56 - Mod Chest Secondary" },
        { BipedSlot::kModShoulder, "57 - Mod Shoulder" },
        { BipedSlot::kModArmLeft, "58 - Mod Arm Left" },
        { BipedSlot::kModArmRight, "59 - Mod Arm Right" },
        { BipedSlot::kModMisc2, "60 - Mod Misc2" },
        { BipedSlot::kFX01, "61 - FX01" }
    } };

    enum class GearColumn : ImGuiID
    {
        Name = 1,
        Plugin,
        Slot
    };

    enum class OutfitColumn : ImGuiID
    {
        Name = 1,
        Plugin,
        Tags,
        Pieces
    };

    int CompareText(std::string_view a_left, std::string_view a_right)
    {
        const auto leftSize = a_left.size();
        const auto rightSize = a_right.size();
        const auto count = (std::min)(leftSize, rightSize);

        for (std::size_t index = 0; index < count; ++index) {
            const auto left = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(a_left[index])));
            const auto right = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(a_right[index])));
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

    std::string CopyCString(const char* a_text)
    {
        if (!a_text || a_text[0] == '\0') {
            return {};
        }
        return a_text;
    }

    std::string FormatFormID(RE::FormID a_formID)
    {
        char buffer[16]{};
        std::snprintf(buffer, sizeof(buffer), "%08X", a_formID);
        return buffer;
    }

    std::string GetDisplayName(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return "Unknown";
        }

        auto name = CopyCString(a_form->GetName());
        if (!name.empty()) {
            return name;
        }

        auto editorID = CopyCString(a_form->GetFormEditorID());
        if (!editorID.empty()) {
            return editorID;
        }

        return "Form " + FormatFormID(a_form->GetFormID());
    }

    std::string JoinStrings(const std::vector<std::string>& a_values)
    {
        std::string output;
        for (const auto& value : a_values) {
            if (!output.empty()) {
                output.append(", ");
            }
            output.append(value);
        }
        return output;
    }

    std::vector<std::string> GetArmorSlotLabels(std::uint64_t a_slotMask)
    {
        std::vector<std::string> labels;
        for (const auto& [slot, label] : kArmorSlotNames) {
            if ((a_slotMask & static_cast<std::uint64_t>(std::to_underlying(slot))) != 0) {
                labels.emplace_back(label);
            }
        }

        if (labels.empty()) {
            labels.emplace_back("None");
        }

        return labels;
    }

    std::string GetPluginName(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return {};
        }

        const auto* file = a_form->GetFile(0);
        if (!file) {
            return {};
        }

        return std::string(file->GetFilename());
    }

    std::string GetFormIdentifier(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return {};
        }

        const auto plugin = GetPluginName(a_form);
        if (plugin.empty()) {
            return {};
        }

        return plugin + "|" + FormatFormID(a_form->GetLocalFormID());
    }

    std::string BuildDavVariantName(const RE::TESObjectARMO* a_sourceArmor, const RE::TESObjectARMO* a_overrideArmor)
    {
        return "SOSNG|" + GetFormIdentifier(a_sourceArmor) + "->" + GetFormIdentifier(a_overrideArmor);
    }

    auto BuildDavConditionsJson() -> std::string
    {
        const nlohmann::json root{
            { "conditions", nlohmann::json::array({ "GetIsReference Player == 1" }) }
        };
        return root.dump();
    }

    auto BuildDavVariantJson(const RE::TESObjectARMO* a_sourceArmor, const RE::TESObjectARMO* a_overrideArmor) -> std::string
    {
        nlohmann::json replaceByForm = nlohmann::json::object();
        std::vector<std::pair<std::uint64_t, std::string>> overrideAddons;
        overrideAddons.reserve(a_overrideArmor->armorAddons.size());

        for (const auto* overrideAddon : a_overrideArmor->armorAddons) {
            if (!overrideAddon) {
                continue;
            }

            const auto identifier = GetFormIdentifier(overrideAddon);
            if (identifier.empty()) {
                continue;
            }

            overrideAddons.emplace_back(overrideAddon->bipedModelData.bipedObjectSlots.underlying(), identifier);
        }

        for (const auto* sourceAddon : a_sourceArmor->armorAddons) {
            if (!sourceAddon) {
                continue;
            }

            const auto sourceIdentifier = GetFormIdentifier(sourceAddon);
            if (sourceIdentifier.empty()) {
                continue;
            }

            const auto sourceSlotMask = sourceAddon->bipedModelData.bipedObjectSlots.underlying();
            std::vector<std::string> replacements;

            for (const auto& [overrideSlotMask, identifier] : overrideAddons) {
                if (overrideSlotMask == sourceSlotMask) {
                    replacements.push_back(identifier);
                }
            }

            if (replacements.empty()) {
                for (const auto& [overrideSlotMask, identifier] : overrideAddons) {
                    if ((overrideSlotMask & sourceSlotMask) != 0) {
                        replacements.push_back(identifier);
                    }
                }
            }

            if (replacements.empty()) {
                for (const auto& [_, identifier] : overrideAddons) {
                    replacements.push_back(identifier);
                }
            }

            if (replacements.empty()) {
                continue;
            }

            if (replacements.size() == 1) {
                replaceByForm[sourceIdentifier] = replacements.front();
            } else {
                replaceByForm[sourceIdentifier] = replacements;
            }
        }

        nlohmann::json root{
            { "displayName", GetDisplayName(a_overrideArmor) },
            { "replaceByForm", replaceByForm }
        };
        return root.dump();
    }

}

namespace sosng
{
    Menu* Menu::GetSingleton()
    {
        static Menu singleton;
        return std::addressof(singleton);
    }

    void Menu::Init(IDXGISwapChain* a_swapChain, ID3D11Device* a_device, ID3D11DeviceContext* a_context)
    {
        if (initialized_) {
            return;
        }

        settingsDirectory_ = kSettingsDirectory;
        imguiIniPath_ = (std::filesystem::path(settingsDirectory_) / kImGuiIniFilename).string();
        userSettingsPath_ = (std::filesystem::path(settingsDirectory_) / kUserSettingsFilename).string();
        LoadUserSettings();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        auto& io = ImGui::GetIO();
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

    void Menu::LoadUserSettings()
    {
        try {
            std::filesystem::create_directories(settingsDirectory_);
        } catch (const std::exception& exception) {
            logger::error("Failed to create settings directory {}: {}", settingsDirectory_, exception.what());
            return;
        }

        std::ifstream input(userSettingsPath_);
        if (!input.is_open()) {
            SaveUserSettings();
            return;
        }

        try {
            const auto json = nlohmann::json::parse(input, nullptr, true, true);
            fontSizePixels_ = std::clamp(json.value("fontSizePx", fontSizePixels_), kMinFontSizePixels, kMaxFontSizePixels);
            pendingFontSizePixels_ = fontSizePixels_;
        } catch (const std::exception& exception) {
            logger::warn("Failed to parse user settings from {}: {}", userSettingsPath_, exception.what());
        }
    }

    void Menu::SaveUserSettings() const
    {
        try {
            std::filesystem::create_directories(settingsDirectory_);
        } catch (const std::exception& exception) {
            logger::error("Failed to create settings directory {}: {}", settingsDirectory_, exception.what());
            return;
        }

        std::ofstream output(userSettingsPath_, std::ios::trunc);
        if (!output.is_open()) {
            logger::error("Failed to write user settings to {}", userSettingsPath_);
            return;
        }

        const nlohmann::json json = {
            { "fontSizePx", fontSizePixels_ }
        };

        output << json.dump(2) << '\n';
    }

    void Menu::RebuildFontAtlas()
    {
        auto& io = ImGui::GetIO();
        auto& style = ImGui::GetStyle();
        ImFontConfig fontConfig{};
        fontConfig.SizePixels = static_cast<float>(fontSizePixels_);
        fontConfig.OversampleH = 1;
        fontConfig.OversampleV = 1;
        fontConfig.PixelSnapH = true;

        io.Fonts->Clear();
        io.FontDefault = nullptr;
        if (std::filesystem::exists(kDefaultFontPath)) {
            io.FontDefault = io.Fonts->AddFontFromFileTTF(kDefaultFontPath, static_cast<float>(fontSizePixels_), &fontConfig);
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

    void Menu::ApplyStyle()
    {
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 12.0f;
        style.FrameRounding = 8.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 8.0f;
        style.FrameBorderSize = 1.0f;
        style.WindowBorderSize = 1.0f;
        style.WindowPadding = { 14.0f, 14.0f };
        style.ItemSpacing = { 10.0f, 8.0f };
        style.CellPadding = { 8.0f, 6.0f };

        auto& colors = style.Colors;
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

    void Menu::Open()
    {
        if (!initialized_) {
            return;
        }

        auto& io = ImGui::GetIO();
        io.MouseDrawCursor = true;
        io.ClearInputKeys();
        enabled_ = true;
    }

    void Menu::Close()
    {
        if (!initialized_) {
            return;
        }

        auto& io = ImGui::GetIO();
        io.MouseDrawCursor = false;
        io.ClearInputKeys();
        if (io.IniFilename) {
            ImGui::SaveIniSettingsToDisk(io.IniFilename);
        }
        SaveUserSettings();
        enabled_ = false;
    }

    void Menu::Toggle()
    {
        if (enabled_) {
            Close();
        } else {
            Open();
        }
    }

    void Menu::Draw()
    {
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

    void Menu::DrawWindow()
    {
        auto& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.50f, io.DisplaySize.y * 0.50f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.50f, io.DisplaySize.y * 0.50f), ImGuiCond_FirstUseEver, ImVec2(0.50f, 0.50f));

        bool open = enabled_;
        if (!ImGui::Begin("Skyrim Outfit System NG", &open, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            if (!open) {
                Close();
            }
            return;
        }

        ImGui::TextUnformatted("Vanity outfit browser");
        ImGui::SameLine();
        ImGui::TextDisabled("| F3 toggles visibility");
        ImGui::Separator();
        ImGui::TextWrapped("Current goal: browse every installed armor and outfit, then wire the visual-swapping layer through a custom Dynamic Armor Variants fork later.");
        ImGui::Spacing();
        ImGui::BulletText("Catalog source: %.*s", static_cast<int>(EquipmentCatalog::Get().GetSource().size()), EquipmentCatalog::Get().GetSource().data());
        ImGui::BulletText("Catalog revision: %.*s", static_cast<int>(EquipmentCatalog::Get().GetRevision().size()), EquipmentCatalog::Get().GetRevision().data());
        ImGui::Separator();

        if (ImGui::BeginTabBar("##catalog-tabs")) {
            if (ImGui::BeginTabItem("Gear")) {
                activeTab_ = BrowserTab::Gear;
                DrawGearTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Outfits")) {
                activeTab_ = BrowserTab::Outfits;
                DrawOutfitTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Options")) {
                activeTab_ = BrowserTab::Options;
                DrawOptionsTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        if (!open) {
            Close();
        }
    }

    bool Menu::MatchesGearFilters(const GearEntry& a_entry) const
    {
        const auto& catalog = EquipmentCatalog::Get();
        if (gearPluginIndex_ > 0 && a_entry.plugin != catalog.GetGearPlugins()[gearPluginIndex_ - 1]) {
            return false;
        }

        if (gearSlotIndex_ > 0 && a_entry.slot != catalog.GetGearSlots()[gearSlotIndex_ - 1]) {
            return false;
        }

        return gearSearch_.PassFilter(a_entry.searchText.c_str());
    }

    bool Menu::MatchesOutfitFilters(const OutfitEntry& a_entry) const
    {
        const auto& catalog = EquipmentCatalog::Get();
        if (outfitPluginIndex_ > 0 && a_entry.plugin != catalog.GetOutfitPlugins()[outfitPluginIndex_ - 1]) {
            return false;
        }

        if (outfitTagIndex_ > 0) {
            const auto selectedTag = catalog.GetOutfitTags()[outfitTagIndex_ - 1];
            if (std::ranges::find(a_entry.tags, selectedTag) == a_entry.tags.end()) {
                return false;
            }
        }

        return outfitSearch_.PassFilter(a_entry.searchText.c_str());
    }

    std::vector<const GearEntry*> Menu::BuildFilteredGear() const
    {
        std::vector<const GearEntry*> rows;
        rows.reserve(EquipmentCatalog::Get().GetGear().size());
        for (const auto& entry : EquipmentCatalog::Get().GetGear()) {
            if (MatchesGearFilters(entry)) {
                rows.push_back(std::addressof(entry));
            }
        }
        return rows;
    }

    std::vector<const OutfitEntry*> Menu::BuildFilteredOutfits() const
    {
        std::vector<const OutfitEntry*> rows;
        rows.reserve(EquipmentCatalog::Get().GetOutfits().size());
        for (const auto& entry : EquipmentCatalog::Get().GetOutfits()) {
            if (MatchesOutfitFilters(entry)) {
                rows.push_back(std::addressof(entry));
            }
        }
        return rows;
    }

    void Menu::SortGearRows(std::vector<const GearEntry*>& a_rows, ImGuiTableSortSpecs* a_sortSpecs) const
    {
        if (!a_sortSpecs || a_sortSpecs->SpecsCount == 0) {
            std::ranges::sort(a_rows, [](const auto* a_left, const auto* a_right) {
                return CompareText(a_left->name, a_right->name) < 0;
            });
            return;
        }

        const auto& spec = a_sortSpecs->Specs[0];
        const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

        std::ranges::sort(a_rows, [&](const auto* a_left, const auto* a_right) {
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

    void Menu::SortOutfitRows(std::vector<const OutfitEntry*>& a_rows, ImGuiTableSortSpecs* a_sortSpecs) const
    {
        if (!a_sortSpecs || a_sortSpecs->SpecsCount == 0) {
            std::ranges::sort(a_rows, [](const auto* a_left, const auto* a_right) {
                return CompareText(a_left->name, a_right->name) < 0;
            });
            return;
        }

        const auto& spec = a_sortSpecs->Specs[0];
        const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

        std::ranges::sort(a_rows, [&](const auto* a_left, const auto* a_right) {
            int compare = 0;
            switch (static_cast<OutfitColumn>(spec.ColumnUserID)) {
            case OutfitColumn::Plugin:
                compare = CompareText(a_left->plugin, a_right->plugin);
                break;
            case OutfitColumn::Tags:
                compare = CompareText(a_left->tagsText, a_right->tagsText);
                break;
            case OutfitColumn::Pieces:
                compare = static_cast<int>(a_left->pieces.size()) - static_cast<int>(a_right->pieces.size());
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

    bool Menu::DrawSearchableStringCombo(const char* a_label, const char* a_allLabel, const std::vector<std::string>& a_options, int& a_index, ImGuiTextFilter& a_filter)
    {
        const char* preview = a_index == 0 ? a_allLabel : a_options[static_cast<std::size_t>(a_index - 1)].c_str();
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
                const auto& option = a_options[index];
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

    void Menu::DrawGearTab()
    {
        const auto& catalog = EquipmentCatalog::Get();

        ImGui::PushItemWidth(260.0f);
        gearSearch_.Draw("Search installed armor", 260.0f);
        ImGui::PopItemWidth();
        ImGui::SameLine();

        DrawSearchableStringCombo("Plugin", "All plugins", catalog.GetGearPlugins(), gearPluginIndex_, gearPluginFilter_);

        ImGui::SameLine();
        if (ImGui::BeginCombo("Slot", gearSlotIndex_ == 0 ? "Any slot" : catalog.GetGearSlots()[gearSlotIndex_ - 1].data())) {
            const bool anySelected = gearSlotIndex_ == 0;
            if (ImGui::Selectable("Any slot", anySelected)) {
                gearSlotIndex_ = 0;
            }
            if (anySelected) {
                ImGui::SetItemDefaultFocus();
            }

            for (std::size_t index = 0; index < catalog.GetGearSlots().size(); ++index) {
                const bool selected = gearSlotIndex_ == static_cast<int>(index + 1);
                if (ImGui::Selectable(catalog.GetGearSlots()[index].data(), selected)) {
                    gearSlotIndex_ = static_cast<int>(index + 1);
                }
            }
            ImGui::EndCombo();
        }

        auto rows = BuildFilteredGear();
        SyncVariantRowsFromPlayer();
        ImGui::Text("Results: %zu", rows.size());
        ImGui::SameLine();
        ImGui::Text("| Equipped rows: %zu", variantRows_.size());

        if (ImGui::BeginTable("##gear-layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp, ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
            ImGui::TableSetupColumn("Catalog", ImGuiTableColumnFlags_WidthStretch, 1.20f);
            ImGui::TableSetupColumn("Variants", ImGuiTableColumnFlags_WidthStretch, 0.95f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (ImGui::BeginChild("##catalog-pane", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
                ImGui::TextUnformatted("Installed armor");
                ImGui::Separator();
                DrawGearCatalogTable(rows);
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginChild("##variant-pane", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
                DrawVariantWorkbenchPane();
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }

        SyncDynamicArmorVariants();
    }

    void Menu::DrawGearCatalogTable(const std::vector<const GearEntry*>& a_rows)
    {
        if (ImGui::BeginTable("##gear-table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, static_cast<ImGuiID>(GearColumn::Name));
            ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_None, 0.0f, static_cast<ImGuiID>(GearColumn::Plugin));
            ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_None, 0.0f, static_cast<ImGuiID>(GearColumn::Slot));
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            auto rows = a_rows;
            SortGearRows(rows, ImGui::TableGetSortSpecs());

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(rows.size()));
            while (clipper.Step()) {
                for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                    const auto& entry = *rows[static_cast<std::size_t>(rowIndex)];
                    EquipmentWidgetItem item{};
                    if (!BuildCatalogItem(entry.formID, item)) {
                        item.formID = entry.formID;
                        item.key = "catalog:" + entry.id;
                        item.name = entry.name;
                        item.slotText = entry.slot;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    DrawEquipmentInfoWidget(item.key.c_str(), item, true, DragSourceKind::Catalog);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(entry.plugin.data());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", entry.slot.data());
                }
            }

            ImGui::EndTable();
        }
    }

    bool Menu::DrawEquipmentInfoWidget(const char* a_id, const EquipmentWidgetItem& a_item, bool a_allowDrag, DragSourceKind a_sourceKind, bool a_showDeleteButton, int a_rowIndex, int a_itemIndex)
    {
        ImGui::PushID(a_id);

        constexpr float paddingX = 10.0f;
        constexpr float paddingY = 7.0f;
        const auto lineHeight = ImGui::GetTextLineHeight();
        const auto frameHeight = paddingY * 2.0f + lineHeight * 2.0f + 4.0f;
        const auto width = ImGui::GetContentRegionAvail().x;
        const auto size = ImVec2(width > 0.0f ? width : 1.0f, frameHeight);
        bool deleteClicked = false;

        ImGui::InvisibleButton("##equipment-widget", size);

        const auto rectMin = ImGui::GetItemRectMin();
        const auto rectMax = ImGui::GetItemRectMax();
        const auto hovered = ImGui::IsItemHovered();
        const auto active = ImGui::IsItemActive();
        auto* drawList = ImGui::GetWindowDrawList();

        ImU32 fillColor = IM_COL32(28, 33, 41, 242);
        if (active) {
            fillColor = IM_COL32(45, 63, 86, 255);
        } else if (hovered) {
            fillColor = IM_COL32(37, 47, 60, 250);
        }

        drawList->AddRectFilled(rectMin, rectMax, fillColor, 8.0f);
        drawList->AddRect(rectMin, rectMax, IM_COL32(85, 103, 122, 255), 8.0f);

        const auto namePos = ImVec2(rectMin.x + paddingX, rectMin.y + paddingY);
        const auto slotPos = ImVec2(rectMin.x + paddingX, rectMin.y + paddingY + lineHeight + 4.0f);
        const auto clipMin = ImVec2(rectMin.x + paddingX, rectMin.y + paddingY);
        const auto buttonWidth = a_showDeleteButton ? 24.0f : 0.0f;
        const auto clipMax = ImVec2(rectMax.x - paddingX - buttonWidth, rectMax.y - paddingY);
        const auto buttonSize = ImVec2(20.0f, 20.0f);
        const auto buttonMin = ImVec2(rectMax.x - paddingX - buttonSize.x, rectMin.y + paddingY);
        const auto buttonMax = ImVec2(buttonMin.x + buttonSize.x, buttonMin.y + buttonSize.y);
        const bool deleteHovered = a_showDeleteButton && ImGui::IsMouseHoveringRect(buttonMin, buttonMax);

        drawList->PushClipRect(clipMin, clipMax, true);
        drawList->AddText(namePos, IM_COL32(235, 239, 244, 255), a_item.name.c_str());
        drawList->AddText(slotPos, IM_COL32(161, 188, 214, 255), a_item.slotText.c_str());
        drawList->PopClipRect();

        if (a_showDeleteButton) {
            const auto deleteFill = deleteHovered ? IM_COL32(122, 52, 52, 255) : IM_COL32(88, 43, 43, 235);
            drawList->AddRectFilled(buttonMin, buttonMax, deleteFill, 4.0f);
            drawList->AddRect(buttonMin, buttonMax, IM_COL32(160, 92, 92, 255), 4.0f);
            drawList->AddText(ImVec2(buttonMin.x + 6.0f, buttonMin.y + 2.0f), IM_COL32(244, 232, 232, 255), "X");

            if (deleteHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                deleteClicked = true;
            }
        }

        if (a_allowDrag && !deleteHovered && ImGui::BeginDragDropSource()) {
            DraggedEquipmentPayload payload{};
            payload.sourceKind = static_cast<std::uint32_t>(a_sourceKind);
            payload.rowIndex = a_rowIndex;
            payload.itemIndex = a_itemIndex;
            payload.formID = a_item.formID;
            ImGui::SetDragDropPayload(kVariantItemPayloadType, &payload, sizeof(payload));
            ImGui::TextUnformatted(a_item.name.c_str());
            ImGui::Text("%s", a_item.slotText.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::PopID();
        return deleteClicked;
    }

    void Menu::SyncVariantRowsFromPlayer()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            variantRows_.clear();
            variantRowOrder_.clear();
            return;
        }

        std::unordered_map<std::string, std::vector<EquipmentWidgetItem>> existingOverrides;
        existingOverrides.reserve(variantRows_.size());
        for (auto& row : variantRows_) {
            existingOverrides.emplace(row.key, std::move(row.overrides));
        }

        std::unordered_map<std::string, VariantWorkbenchRow> rowMap;
        rowMap.reserve(variantRows_.size() + 8);
        std::vector<std::string> discoveredKeys;
        discoveredKeys.reserve(variantRows_.size() + 8);
        std::unordered_set<RE::FormID> seenArmorForms;

        for (const auto& [slot, label] : kArmorSlotNames) {
            auto* armor = player->GetWornArmor(slot);
            if (!armor) {
                continue;
            }

            const auto formID = armor->GetFormID();
            if (!seenArmorForms.insert(formID).second) {
                continue;
            }

            EquipmentWidgetItem equipped{};
            if (!BuildCatalogItem(formID, equipped)) {
                continue;
            }

            VariantWorkbenchRow row{};
            row.key = "armor:" + FormatFormID(formID);
            row.equipped = std::move(equipped);
            row.equipped.key = row.key;
            if (const auto it = existingOverrides.find(row.key); it != existingOverrides.end()) {
                row.overrides = std::move(it->second);
            }
            discoveredKeys.push_back(row.key);
            rowMap.emplace(row.key, std::move(row));
        }

        std::vector<VariantWorkbenchRow> rows;
        rows.reserve(rowMap.size());
        std::unordered_set<std::string> placedKeys;
        placedKeys.reserve(rowMap.size());

        for (const auto& key : variantRowOrder_) {
            const auto it = rowMap.find(key);
            if (it == rowMap.end()) {
                continue;
            }

            placedKeys.insert(key);
            rows.push_back(std::move(it->second));
        }

        for (const auto& key : discoveredKeys) {
            if (!placedKeys.insert(key).second) {
                continue;
            }

            const auto it = rowMap.find(key);
            if (it == rowMap.end()) {
                continue;
            }

            rows.push_back(std::move(it->second));
        }

        variantRowOrder_.clear();
        variantRowOrder_.reserve(rows.size());
        for (const auto& row : rows) {
            variantRowOrder_.push_back(row.key);
        }

        variantRows_ = std::move(rows);
    }

    bool Menu::BuildCatalogItem(RE::FormID a_formID, EquipmentWidgetItem& a_item) const
    {
        const auto* form = RE::TESForm::LookupByID(a_formID);
        if (!form) {
            return false;
        }

        a_item = {};
        a_item.formID = form->GetFormID();
        a_item.key = "form:" + FormatFormID(form->GetFormID());
        a_item.name = GetDisplayName(form);

        if (const auto* armor = form->As<RE::TESObjectARMO>()) {
            a_item.slotMask = armor->GetSlotMask().underlying();
            a_item.slotText = JoinStrings(GetArmorSlotLabels(a_item.slotMask));
            return true;
        }

        return false;
    }

    bool Menu::CanAcceptOverride(int a_targetRowIndex, const EquipmentWidgetItem& a_item, int a_sourceRowIndex, int a_sourceItemIndex) const
    {
        if (a_targetRowIndex < 0 || a_targetRowIndex >= static_cast<int>(variantRows_.size())) {
            return false;
        }

        if (a_sourceRowIndex == a_targetRowIndex && a_sourceItemIndex >= 0) {
            return false;
        }

        const auto& row = variantRows_[static_cast<std::size_t>(a_targetRowIndex)];
        if (a_item.slotMask == 0) {
            return false;
        }

        for (int itemIndex = 0; itemIndex < static_cast<int>(row.overrides.size()); ++itemIndex) {
            if (a_targetRowIndex == a_sourceRowIndex && itemIndex == a_sourceItemIndex) {
                continue;
            }

            const auto& existing = row.overrides[static_cast<std::size_t>(itemIndex)];
            if ((existing.slotMask & a_item.slotMask) != 0) {
                return false;
            }
        }

        return true;
    }

    void Menu::AcceptOverridePayload(int a_targetRowIndex)
    {
        const auto* payload = ImGui::AcceptDragDropPayload(kVariantItemPayloadType);
        if (!payload || payload->DataSize != sizeof(DraggedEquipmentPayload)) {
            return;
        }

        DraggedEquipmentPayload dragPayload{};
        std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));

        EquipmentWidgetItem item{};
        if (dragPayload.sourceKind == static_cast<std::uint32_t>(DragSourceKind::Override)) {
            if (dragPayload.rowIndex < 0 || dragPayload.rowIndex >= static_cast<int>(variantRows_.size())) {
                return;
            }

            const auto& sourceOverrides = variantRows_[static_cast<std::size_t>(dragPayload.rowIndex)].overrides;
            if (dragPayload.itemIndex < 0 || dragPayload.itemIndex >= static_cast<int>(sourceOverrides.size())) {
                return;
            }

            item = sourceOverrides[static_cast<std::size_t>(dragPayload.itemIndex)];
        } else {
            if (!BuildCatalogItem(dragPayload.formID, item)) {
                return;
            }
        }

        if (!CanAcceptOverride(a_targetRowIndex, item, dragPayload.rowIndex, dragPayload.itemIndex)) {
            return;
        }

        if (dragPayload.sourceKind == static_cast<std::uint32_t>(DragSourceKind::Override)) {
            auto& sourceOverrides = variantRows_[static_cast<std::size_t>(dragPayload.rowIndex)].overrides;
            sourceOverrides.erase(sourceOverrides.begin() + dragPayload.itemIndex);
        }

        variantRows_[static_cast<std::size_t>(a_targetRowIndex)].overrides.push_back(std::move(item));
    }

    void Menu::ApplyRowReorder(const DraggedEquipmentPayload& a_dragPayload, int a_targetRowIndex, bool a_insertAfter)
    {
        if (a_dragPayload.sourceKind != static_cast<std::uint32_t>(DragSourceKind::Row)) {
            return;
        }

        if (a_dragPayload.rowIndex < 0 || a_dragPayload.rowIndex >= static_cast<int>(variantRows_.size())) {
            return;
        }
        if (a_targetRowIndex < 0 || a_targetRowIndex >= static_cast<int>(variantRows_.size())) {
            return;
        }
        if (a_dragPayload.rowIndex == a_targetRowIndex) {
            return;
        }

        auto movedRow = std::move(variantRows_[static_cast<std::size_t>(a_dragPayload.rowIndex)]);
        variantRows_.erase(variantRows_.begin() + a_dragPayload.rowIndex);

        auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
        if (a_dragPayload.rowIndex < insertIndex) {
            --insertIndex;
        }

        variantRows_.insert(variantRows_.begin() + insertIndex, std::move(movedRow));

        variantRowOrder_.clear();
        variantRowOrder_.reserve(variantRows_.size());
        for (const auto& row : variantRows_) {
            variantRowOrder_.push_back(row.key);
        }
    }

    void Menu::AcceptOverrideDeletePayload()
    {
        const auto* payload = ImGui::AcceptDragDropPayload(kVariantItemPayloadType);
        if (!payload || payload->DataSize != sizeof(DraggedEquipmentPayload)) {
            return;
        }

        DraggedEquipmentPayload dragPayload{};
        std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
        if (dragPayload.sourceKind != static_cast<std::uint32_t>(DragSourceKind::Override)) {
            return;
        }

        if (dragPayload.rowIndex < 0 || dragPayload.rowIndex >= static_cast<int>(variantRows_.size())) {
            return;
        }

        auto& overrides = variantRows_[static_cast<std::size_t>(dragPayload.rowIndex)].overrides;
        if (dragPayload.itemIndex < 0 || dragPayload.itemIndex >= static_cast<int>(overrides.size())) {
            return;
        }

        overrides.erase(overrides.begin() + dragPayload.itemIndex);
    }

    void Menu::SyncDynamicArmorVariants()
    {
        using integrations::DynamicArmorVariantsClient;

        auto* dav = DynamicArmorVariantsClient::Get();
        if (!dav || !dav->IsReady()) {
            return;
        }

        const auto conditionsJson = BuildDavConditionsJson();

        struct DavVariantPayload
        {
            std::string variantJson;
        };

        std::unordered_map<std::string, DavVariantPayload> desiredVariants;
        desiredVariants.reserve(variantRows_.size() * 2);

        for (const auto& row : variantRows_) {
            const auto* sourceArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(row.equipped.formID);
            if (!sourceArmor) {
                continue;
            }

            for (const auto& overrideItem : row.overrides) {
                const auto* overrideArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(overrideItem.formID);
                if (!overrideArmor) {
                    continue;
                }

                const auto variantName = BuildDavVariantName(sourceArmor, overrideArmor);
                if (variantName.empty()) {
                    continue;
                }

                desiredVariants.try_emplace(variantName, DavVariantPayload{ BuildDavVariantJson(sourceArmor, overrideArmor) });
            }
        }

        std::unordered_set<std::string> syncedNames;
        syncedNames.reserve(desiredVariants.size());
        bool variantsChanged = false;

        for (const auto& [variantName, payload] : desiredVariants) {
            if (activeDavVariantNames_.contains(variantName)) {
                syncedNames.insert(variantName);
                continue;
            }

            if (!dav->RegisterVariantJson(variantName.c_str(), payload.variantJson.c_str())) {
                logger::warn("Failed to register DAV variant {}", variantName);
                continue;
            }

            if (!dav->SetVariantConditionsJson(variantName.c_str(), conditionsJson.c_str())) {
                logger::warn("Failed to set DAV conditions for {}", variantName);
                dav->DeleteVariant(variantName.c_str());
                continue;
            }

            syncedNames.insert(variantName);
            variantsChanged = true;
        }

        for (const auto& variantName : activeDavVariantNames_) {
            if (syncedNames.contains(variantName)) {
                continue;
            }

            if (!dav->DeleteVariant(variantName.c_str())) {
                logger::warn("Failed to delete DAV variant {}", variantName);
            } else {
                variantsChanged = true;
            }
        }

        activeDavVariantNames_ = std::move(syncedNames);

        if (variantsChanged) {
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                dav->RefreshActor(player);
            }
        }
    }

    void Menu::DrawVariantWorkbenchPane()
    {
        ImGui::TextUnformatted("Variant workbench");
        ImGui::Separator();
        ImGui::TextWrapped("Each row is a currently equipped armor piece. Drag the left column to reorder rows. Drop armor into the Overrides column. Only overlapping override slots on the same row are rejected.");
        ImGui::Spacing();

        if (variantRows_.empty()) {
            ImGui::TextWrapped("No equipped armor pieces were found on the player.");
            return;
        }

        constexpr float deletePaneHeight = 86.0f;
        const auto tableHeight = (std::max)(0.0f, ImGui::GetContentRegionAvail().y - deletePaneHeight);
        if (ImGui::BeginChild("##variant-workbench-scroll", ImVec2(0.0f, tableHeight), ImGuiChildFlags_None)) {
            if (ImGui::BeginTable("##variant-workbench", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 0.0f))) {
                ImGui::TableSetupColumn("Equipped", ImGuiTableColumnFlags_WidthStretch, 0.85f);
                ImGui::TableSetupColumn("Overrides", ImGuiTableColumnFlags_WidthStretch, 1.15f);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                const auto* activePayload = ImGui::GetDragDropPayload();
                const bool rowDragActive =
                    activePayload &&
                    activePayload->Data != nullptr &&
                    activePayload->IsDataType(kVariantItemPayloadType) &&
                    activePayload->DataSize == sizeof(DraggedEquipmentPayload) &&
                    static_cast<const DraggedEquipmentPayload*>(activePayload->Data)->sourceKind == static_cast<std::uint32_t>(DragSourceKind::Row);
                float insertionLineY = -1.0f;
                float insertionLineX1 = -1.0f;
                float insertionLineX2 = -1.0f;
                std::optional<DraggedEquipmentPayload> acceptedRowReorderPayload;
                int acceptedRowReorderIndex = -1;
                bool acceptedRowInsertAfter = false;
                int hoveredRowReorderIndex = -1;
                bool hoveredRowInsertAfter = false;
                std::vector<float> rowTopY;
                std::vector<float> rowBottomY;
                rowTopY.reserve(variantRows_.size());
                rowBottomY.reserve(variantRows_.size());

                for (int rowIndex = 0; rowIndex < static_cast<int>(variantRows_.size()); ++rowIndex) {
                    const auto overrideCount = variantRows_[static_cast<std::size_t>(rowIndex)].overrides.size();
                    const auto dropZoneHeight = (std::max)(72.0f, 14.0f + static_cast<float>(overrideCount) * (ImGui::GetTextLineHeightWithSpacing() * 2.5f));
                    const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
                    const auto rowHeight = (std::max)(widgetHeight, dropZoneHeight);
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

                    ImGui::TableSetColumnIndex(0);
                    DrawEquipmentInfoWidget(variantRows_[static_cast<std::size_t>(rowIndex)].key.c_str(), variantRows_[static_cast<std::size_t>(rowIndex)].equipped, true, DragSourceKind::Row, false, rowIndex);
                    const auto* table = ImGui::GetCurrentTable();
                    if (table) {
                        const ImRect leftCellRect = ImGui::TableGetCellBgRect(table, 0);
                        rowTopY.push_back(leftCellRect.Min.y);
                        const bool insertAfter = ImGui::GetIO().MousePos.y > ((leftCellRect.Min.y + leftCellRect.Max.y) * 0.5f);

                        if (rowDragActive && ImGui::IsMouseHoveringRect(leftCellRect.Min, leftCellRect.Max)) {
                            hoveredRowReorderIndex = rowIndex;
                            hoveredRowInsertAfter = insertAfter;
                            insertionLineX1 = table->OuterRect.Min.x + 2.0f;
                            insertionLineX2 = table->OuterRect.Max.x - 2.0f;
                        }

                        if (ImGui::BeginDragDropTargetCustom(leftCellRect, ImGui::GetID(("##row-reorder-" + std::to_string(rowIndex)).c_str()))) {
                            if (const auto* payload = ImGui::AcceptDragDropPayload(kVariantItemPayloadType, ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                                payload && payload->Data != nullptr && payload->DataSize == sizeof(DraggedEquipmentPayload)) {
                                DraggedEquipmentPayload dragPayload{};
                                std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
                                if (dragPayload.sourceKind == static_cast<std::uint32_t>(DragSourceKind::Row)) {
                                    acceptedRowReorderPayload = dragPayload;
                                    acceptedRowReorderIndex = rowIndex;
                                    acceptedRowInsertAfter = insertAfter;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }

                    ImGui::TableSetColumnIndex(1);
                    const auto dropId = "##override-drop-" + std::to_string(rowIndex);
                    ImGui::BeginChild(dropId.c_str(), ImVec2(0.0f, dropZoneHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
                    const auto childPos = ImGui::GetWindowPos();
                    const auto childSize = ImGui::GetWindowSize();

                    if (overrideCount == 0) {
                        ImGui::TextWrapped("Drop equipment overrides here.");
                    } else {
                        for (int overrideIndex = 0; overrideIndex < static_cast<int>(overrideCount); ++overrideIndex) {
                            if (overrideIndex > 0) {
                                ImGui::Spacing();
                            }

                            const auto widgetId = "override:" + std::to_string(rowIndex) + ":" + std::to_string(overrideIndex);
                            if (DrawEquipmentInfoWidget(widgetId.c_str(), variantRows_[static_cast<std::size_t>(rowIndex)].overrides[static_cast<std::size_t>(overrideIndex)], true, DragSourceKind::Override, true, rowIndex, overrideIndex)) {
                                variantRows_[static_cast<std::size_t>(rowIndex)].overrides.erase(
                                    variantRows_[static_cast<std::size_t>(rowIndex)].overrides.begin() + overrideIndex);
                                break;
                            }
                        }
                    }

                    const ImRect dropRect(
                        childPos,
                        ImVec2(childPos.x + childSize.x, childPos.y + childSize.y));
                    if (ImGui::BeginDragDropTargetCustom(dropRect, ImGui::GetID("##override-cell-target"))) {
                        AcceptOverridePayload(rowIndex);
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::EndChild();

                    if (const auto* rowTable = ImGui::GetCurrentTable()) {
                        rowBottomY.push_back(dropRect.Max.y + rowTable->RowCellPaddingY);
                    } else {
                        rowBottomY.push_back(dropRect.Max.y);
                    }
                }

                if (hoveredRowReorderIndex >= 0 && hoveredRowReorderIndex < static_cast<int>(rowTopY.size()) &&
                    hoveredRowReorderIndex < static_cast<int>(rowBottomY.size())) {
                    insertionLineY = hoveredRowInsertAfter ?
                                         rowBottomY[static_cast<std::size_t>(hoveredRowReorderIndex)] :
                                         rowTopY[static_cast<std::size_t>(hoveredRowReorderIndex)];
                }

                if (rowDragActive && insertionLineY >= 0.0f && insertionLineX2 > insertionLineX1) {
                    auto* drawList = ImGui::GetWindowDrawList();
                    drawList->AddLine(
                        ImVec2(insertionLineX1, insertionLineY),
                        ImVec2(insertionLineX2, insertionLineY),
                        IM_COL32(116, 189, 255, 255),
                        2.0f);
                }

                ImGui::EndTable();

                if (acceptedRowReorderPayload && acceptedRowReorderIndex >= 0) {
                    ApplyRowReorder(*acceptedRowReorderPayload, acceptedRowReorderIndex, acceptedRowInsertAfter);
                }
            }
            ImGui::EndChild();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::BeginChild("##override-delete-target", ImVec2(0.0f, 56.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const auto deletePos = ImGui::GetWindowPos();
        const auto deleteSize = ImGui::GetWindowSize();
        ImGui::TextWrapped("Drag an override here to delete it.");
        const ImRect deleteRect(
            deletePos,
            ImVec2(deletePos.x + deleteSize.x, deletePos.y + deleteSize.y));
        if (ImGui::BeginDragDropTargetCustom(deleteRect, ImGui::GetID("##override-delete-drop"))) {
            AcceptOverrideDeletePayload();
            ImGui::EndDragDropTarget();
        }
        ImGui::EndChild();
    }

    void Menu::DrawOutfitTab()
    {
        const auto& catalog = EquipmentCatalog::Get();

        ImGui::PushItemWidth(260.0f);
        outfitSearch_.Draw("Search installed outfits", 260.0f);
        ImGui::PopItemWidth();
        ImGui::SameLine();

        DrawSearchableStringCombo("Plugin", "All plugins", catalog.GetOutfitPlugins(), outfitPluginIndex_, outfitPluginFilter_);

        ImGui::SameLine();
        if (ImGui::BeginCombo("Tag", outfitTagIndex_ == 0 ? "Any tag" : catalog.GetOutfitTags()[outfitTagIndex_ - 1].data())) {
            const bool anySelected = outfitTagIndex_ == 0;
            if (ImGui::Selectable("Any tag", anySelected)) {
                outfitTagIndex_ = 0;
            }
            if (anySelected) {
                ImGui::SetItemDefaultFocus();
            }

            for (std::size_t index = 0; index < catalog.GetOutfitTags().size(); ++index) {
                const bool selected = outfitTagIndex_ == static_cast<int>(index + 1);
                if (ImGui::Selectable(catalog.GetOutfitTags()[index].data(), selected)) {
                    outfitTagIndex_ = static_cast<int>(index + 1);
                }
            }
            ImGui::EndCombo();
        }

        auto rows = BuildFilteredOutfits();
        ImGui::Text("Results: %zu", rows.size());

        const auto tableHeight = ImGui::GetContentRegionAvail().y;
        if (ImGui::BeginTable("##outfit-table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, tableHeight))) {
            ImGui::TableSetupColumn("Outfit", ImGuiTableColumnFlags_DefaultSort, 0.0f, static_cast<ImGuiID>(OutfitColumn::Name));
            ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_None, 0.0f, static_cast<ImGuiID>(OutfitColumn::Plugin));
            ImGui::TableSetupColumn("Tags", ImGuiTableColumnFlags_None, 0.0f, static_cast<ImGuiID>(OutfitColumn::Tags));
            ImGui::TableSetupColumn("Pieces", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, static_cast<ImGuiID>(OutfitColumn::Pieces));
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            SortOutfitRows(rows, ImGui::TableGetSortSpecs());

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(rows.size()));
            while (clipper.Step()) {
                for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                    const auto& outfit = *rows[static_cast<std::size_t>(rowIndex)];

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(outfit.name.data());
                    ImGui::TextDisabled("%s", outfit.editorID.data());
                    ImGui::TextWrapped("%s", outfit.summary.data());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(outfit.plugin.data());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextWrapped("%s", outfit.tagsText.c_str());

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%zu", outfit.pieces.size());
                    ImGui::TextDisabled("%s", outfit.piecesText.c_str());
                }
            }

            ImGui::EndTable();
        }
    }

    void Menu::DrawOptionsTab()
    {
        ImGui::TextUnformatted("Interface");
        ImGui::Separator();
        ImGui::TextWrapped("Adjust the UI font size for this session. The value is stored in integer pixels and the font atlas rebuild is applied after you finish editing.");

        ImGui::SliderInt("Font size", &pendingFontSizePixels_, kMinFontSizePixels, kMaxFontSizePixels, "%d px");
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
}
