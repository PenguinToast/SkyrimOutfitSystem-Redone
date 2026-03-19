#include "integrations/DynamicArmorVariantsClient.h"

namespace sosng::integrations
{
    void DynamicArmorVariantsClient::Refresh()
    {
        interface_ = DynamicArmorVariantsAPI::GetDynamicArmorVariantsInterface001();
        attemptedFetch_ = true;

        if (!interface_) {
            if (!availabilityWarningLogged_) {
                logger::warn("Dynamic Armor Variants API not available");
                availabilityWarningLogged_ = true;
            }
            return;
        }

        availabilityWarningLogged_ = false;
        logger::info("Dynamic Armor Variants API handle acquired");
    }

    auto DynamicArmorVariantsClient::Get() -> DynamicArmorVariantsAPI::IDynamicArmorVariantsInterface001*
    {
        if (!interface_) {
            Refresh();
        }

        return interface_;
    }

    auto DynamicArmorVariantsClient::IsAvailable() -> bool
    {
        return Get() != nullptr;
    }
}
