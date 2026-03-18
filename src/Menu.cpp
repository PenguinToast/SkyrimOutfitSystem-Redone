#include "Menu.h"

#include "InputManager.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace
{
    constexpr auto kSettingsDirectory = "Data/SKSE/Plugins/SkyrimOutfitSystemNG";
    constexpr auto kImGuiIniFilename = "imgui.ini";
    constexpr auto kUserSettingsFilename = "settings.json";
    constexpr int kDefaultFontSizePixels = 13;
    constexpr int kMinFontSizePixels = 8;
    constexpr int kMaxFontSizePixels = 28;

    enum class GearColumn : ImGuiID
    {
        Name = 1,
        Plugin,
        Slot,
        Stat,
        Weight,
        Value
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

        io.Fonts->Clear();
        io.FontDefault = io.Fonts->AddFontDefaultVector(&fontConfig);
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
        ImGui::TextWrapped("Current goal: browse every installed armor, weapon, and outfit, then wire the visual-swapping layer through a custom Dynamic Armor Variants fork later.");
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
        if (gearKindFilter_ == GearKindFilter::Armor && a_entry.kind != GearKind::Armor) {
            return false;
        }
        if (gearKindFilter_ == GearKindFilter::Weapon && a_entry.kind != GearKind::Weapon) {
            return false;
        }

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
            case GearColumn::Stat:
                compare = a_left->statValue - a_right->statValue;
                break;
            case GearColumn::Weight:
                compare = a_left->weight < a_right->weight ? -1 : (a_left->weight > a_right->weight ? 1 : 0);
                break;
            case GearColumn::Value:
                compare = a_left->value - a_right->value;
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
        gearSearch_.Draw("Search installed gear", 260.0f);
        ImGui::PopItemWidth();
        ImGui::SameLine();

        DrawSearchableStringCombo("Plugin", "All plugins", catalog.GetGearPlugins(), gearPluginIndex_, gearPluginFilter_);

        ImGui::SameLine();
        if (ImGui::BeginCombo("Slot / class", gearSlotIndex_ == 0 ? "Any slot" : catalog.GetGearSlots()[gearSlotIndex_ - 1].data())) {
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

        int gearKindSelection = static_cast<int>(gearKindFilter_);
        for (std::size_t index = 0; index < kGearKindLabels.size(); ++index) {
            if (index > 0) {
                ImGui::SameLine();
            }
            ImGui::RadioButton(kGearKindLabels[index].data(), &gearKindSelection, static_cast<int>(index));
        }
        gearKindFilter_ = static_cast<GearKindFilter>(gearKindSelection);

        auto rows = BuildFilteredGear();
        ImGui::Text("Results: %zu", rows.size());

        const auto tableHeight = ImGui::GetContentRegionAvail().y;
        if (ImGui::BeginTable("##gear-table", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, tableHeight))) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, static_cast<ImGuiID>(GearColumn::Name));
            ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_None, 0.0f, static_cast<ImGuiID>(GearColumn::Plugin));
            ImGui::TableSetupColumn("Slot / class", ImGuiTableColumnFlags_None, 0.0f, static_cast<ImGuiID>(GearColumn::Slot));
            ImGui::TableSetupColumn("Armor / damage", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, static_cast<ImGuiID>(GearColumn::Stat));
            ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, static_cast<ImGuiID>(GearColumn::Weight));
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, static_cast<ImGuiID>(GearColumn::Value));
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            SortGearRows(rows, ImGui::TableGetSortSpecs());

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(rows.size()));
            while (clipper.Step()) {
                for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                    const auto& entry = *rows[static_cast<std::size_t>(rowIndex)];

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(entry.name.data());
                    ImGui::TextDisabled("%s", entry.editorID.data());
                    ImGui::TextDisabled("%s", entry.keywordsText.c_str());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(entry.plugin.data());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s / %s", entry.slot.data(), entry.category.data());

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%d", entry.statValue);

                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%.1f", entry.weight);

                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%d", entry.value);
                }
            }

            ImGui::EndTable();
        }
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
