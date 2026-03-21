#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace DynamicArmorVariantsExtendedAPI {
constexpr auto DynamicArmorVariantsExtendedPluginName = "DynamicArmorVariants";

struct DynamicArmorVariantsExtendedMessage {
  enum : std::uint32_t { kMessage_QueryInterface = 'DAVX' };

  void *(*GetApiFunction)(unsigned int revisionNumber) = nullptr;
};

struct IDynamicArmorVariantsExtendedInterface001;
IDynamicArmorVariantsExtendedInterface001 *
GetDynamicArmorVariantsExtendedInterface001();

struct IDynamicArmorVariantsExtendedInterface001 {
  virtual bool IsReady() = 0;
  virtual bool RegisterVariantJson(const char *a_name,
                                   const char *a_variantJson) = 0;
  virtual bool DeleteVariant(const char *a_name) = 0;
  virtual bool SetVariantConditionsJson(const char *a_name,
                                        const char *a_conditionsJson) = 0;
  virtual bool RefreshActor(RE::Actor *a_actor) = 0;
  virtual bool ApplyVariantOverride(RE::Actor *a_actor, const char *a_variant,
                                    bool a_keepExistingOverrides = false) = 0;
  virtual bool RemoveVariantOverride(RE::Actor *a_actor,
                                     const char *a_variant) = 0;
};
} // namespace DynamicArmorVariantsExtendedAPI

extern DynamicArmorVariantsExtendedAPI::
    IDynamicArmorVariantsExtendedInterface001
        *g_DynamicArmorVariantsExtendedInterface;
