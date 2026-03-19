#pragma once

#include "integrations/DynamicArmorVariantsAPI.h"

namespace sosng::integrations
{
    class DynamicArmorVariantsClient
    {
    public:
        static void Refresh();
        static auto Get() -> DynamicArmorVariantsAPI::IDynamicArmorVariantsInterface001*;
        static auto IsAvailable() -> bool;

    private:
        static inline DynamicArmorVariantsAPI::IDynamicArmorVariantsInterface001* interface_{ nullptr };
        static inline bool attemptedFetch_{ false };
        static inline bool availabilityWarningLogged_{ false };
    };
}
