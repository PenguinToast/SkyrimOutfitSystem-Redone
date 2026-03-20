#pragma once

#include "integrations/DynamicArmorVariantsExtendedAPI.h"

namespace sosr::integrations {
class DynamicArmorVariantsExtendedClient {
public:
  static void Refresh();
  static auto Get() -> DynamicArmorVariantsExtendedAPI::
      IDynamicArmorVariantsExtendedInterface001 *;
  static auto IsAvailable() -> bool;

private:
  static inline DynamicArmorVariantsExtendedAPI::
      IDynamicArmorVariantsExtendedInterface001 *interface_{nullptr};
  static inline bool attemptedFetch_{false};
  static inline bool availabilityWarningLogged_{false};
};
} // namespace sosr::integrations
