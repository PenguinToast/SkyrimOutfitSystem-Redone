#include "integrations/DynamicArmorVariantsExtendedClient.h"

namespace sosr::integrations {
void DynamicArmorVariantsExtendedClient::Refresh() {
  interface_ = DynamicArmorVariantsExtendedAPI::
      GetDynamicArmorVariantsExtendedInterface001();
  attemptedFetch_ = true;

  if (!interface_) {
    if (!availabilityWarningLogged_) {
      logger::warn("Dynamic Armor Variants Extended API not available");
      availabilityWarningLogged_ = true;
    }
    return;
  }

  availabilityWarningLogged_ = false;
  logger::info("Dynamic Armor Variants Extended API handle acquired");
}

auto DynamicArmorVariantsExtendedClient::Get()
    -> DynamicArmorVariantsExtendedAPI::
        IDynamicArmorVariantsExtendedInterface001 * {
  if (!interface_) {
    Refresh();
  }

  return interface_;
}

auto DynamicArmorVariantsExtendedClient::IsAvailable() -> bool {
  return Get() != nullptr;
}
} // namespace sosr::integrations
