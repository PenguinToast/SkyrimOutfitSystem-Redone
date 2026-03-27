#pragma once

#include "integrations/DynamicArmorVariantsExtendedAPI.h"

namespace sosr::integrations {
class DynamicArmorVariantsExtendedClient {
public:
  enum class Status { Unavailable, VersionTooOld, NotReady, Ready };

  static void Initialize(const SKSE::LoadInterface *a_loadInterface);
  static void Refresh();
  static auto Get() -> DynamicArmorVariantsExtendedAPI::
      IDynamicArmorVariantsExtendedInterface001 *;
  static auto GetReady() -> DynamicArmorVariantsExtendedAPI::
      IDynamicArmorVariantsExtendedInterface001 *;
  static auto IsAvailable() -> bool;
  static auto HasMinimumVersion() -> bool;
  static auto GetDetectedVersion() -> std::optional<REL::Version>;
  static auto GetStatus() -> Status;
  static auto GetAvailabilityMessage() -> std::optional<std::string>;

private:
  static void RefreshDetectedVersion();

  static inline DynamicArmorVariantsExtendedAPI::
      IDynamicArmorVariantsExtendedInterface001 *interface_{nullptr};
  static inline const SKSE::LoadInterface *loadInterface_{nullptr};
  static inline std::optional<REL::Version> detectedVersion_{std::nullopt};
  static inline bool attemptedFetch_{false};
  static inline bool availabilityWarningLogged_{false};
};
} // namespace sosr::integrations
