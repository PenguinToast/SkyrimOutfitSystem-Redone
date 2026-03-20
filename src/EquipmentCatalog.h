#pragma once

namespace sosng {
struct GearEntry {
  RE::FormID formID;
  std::string id;
  std::string name;
  std::string editorID;
  std::string plugin;
  std::string category;
  std::string slot;
  int statValue;
  float weight;
  int value;
  std::vector<std::string> keywords;
  std::string keywordsText;
  std::string searchText;
};

struct OutfitEntry {
  RE::FormID formID;
  std::string id;
  std::string name;
  std::string editorID;
  std::string plugin;
  std::string summary;
  std::vector<std::string> pieces;
  std::vector<std::string> tags;
  std::string piecesText;
  std::string tagsText;
  std::string searchText;
};

class EquipmentCatalog {
public:
  static EquipmentCatalog &Get();

  void RefreshFromGame();

  [[nodiscard]] const std::vector<GearEntry> &GetGear() const { return gear_; }
  [[nodiscard]] const std::vector<OutfitEntry> &GetOutfits() const {
    return outfits_;
  }

  [[nodiscard]] const std::vector<std::string> &GetGearPlugins() const {
    return gearPlugins_;
  }
  [[nodiscard]] const std::vector<std::string> &GetGearSlots() const {
    return gearSlots_;
  }
  [[nodiscard]] const std::vector<std::string> &GetOutfitPlugins() const {
    return outfitPlugins_;
  }
  [[nodiscard]] const std::vector<std::string> &GetOutfitTags() const {
    return outfitTags_;
  }

  [[nodiscard]] std::string_view GetSource() const { return source_; }
  [[nodiscard]] std::string_view GetRevision() const { return revision_; }

private:
  EquipmentCatalog();
  void RebuildDerivedData();

  std::vector<GearEntry> gear_;
  std::vector<OutfitEntry> outfits_;
  std::vector<std::string> gearPlugins_;
  std::vector<std::string> gearSlots_;
  std::vector<std::string> outfitPlugins_;
  std::vector<std::string> outfitTags_;
  std::string source_;
  std::string revision_;
};
} // namespace sosng
