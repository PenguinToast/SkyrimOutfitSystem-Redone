#include "integrations/DynamicArmorVariantsExtendedAPI.h"

DynamicArmorVariantsExtendedAPI::IDynamicArmorVariantsExtendedInterface001
    *g_DynamicArmorVariantsExtendedInterface = nullptr;

DynamicArmorVariantsExtendedAPI::IDynamicArmorVariantsExtendedInterface001 *
DynamicArmorVariantsExtendedAPI::GetDynamicArmorVariantsExtendedInterface001() {
  if (g_DynamicArmorVariantsExtendedInterface) {
    return g_DynamicArmorVariantsExtendedInterface;
  }

  DynamicArmorVariantsExtendedMessage message;
  auto *messaging = SKSE::GetMessagingInterface();
  messaging->Dispatch(
      DynamicArmorVariantsExtendedMessage::kMessage_QueryInterface,
      std::addressof(message), sizeof(DynamicArmorVariantsExtendedMessage),
      DynamicArmorVariantsExtendedPluginName);

  if (!message.GetApiFunction) {
    return nullptr;
  }

  g_DynamicArmorVariantsExtendedInterface =
      static_cast<IDynamicArmorVariantsExtendedInterface001 *>(
          message.GetApiFunction(1));
  return g_DynamicArmorVariantsExtendedInterface;
}
