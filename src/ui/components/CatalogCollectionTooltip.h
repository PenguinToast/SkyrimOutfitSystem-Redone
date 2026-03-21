#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace sosr::ui::components {
struct CatalogTooltipMetaRow {
  const char *icon{""};
  std::string label;
  std::string value;
};

struct CatalogTooltipItemRow {
  std::string name;
  std::string slots;
};

void DrawCatalogCollectionTooltip(
    std::string_view a_title,
    const std::vector<CatalogTooltipMetaRow> &a_metaRows,
    const std::vector<CatalogTooltipItemRow> &a_items);
} // namespace sosr::ui::components
