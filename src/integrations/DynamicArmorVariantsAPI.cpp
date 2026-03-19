#include "integrations/DynamicArmorVariantsAPI.h"

DynamicArmorVariantsAPI::IDynamicArmorVariantsInterface001* g_DynamicArmorVariantsInterface = nullptr;

DynamicArmorVariantsAPI::IDynamicArmorVariantsInterface001* DynamicArmorVariantsAPI::GetDynamicArmorVariantsInterface001()
{
    if (g_DynamicArmorVariantsInterface) {
        return g_DynamicArmorVariantsInterface;
    }

    DynamicArmorVariantsMessage message;
    auto* messaging = SKSE::GetMessagingInterface();
    messaging->Dispatch(
        DynamicArmorVariantsMessage::kMessage_QueryInterface,
        std::addressof(message),
        sizeof(DynamicArmorVariantsMessage),
        DynamicArmorVariantsPluginName);

    if (!message.GetApiFunction) {
        return nullptr;
    }

    g_DynamicArmorVariantsInterface = static_cast<IDynamicArmorVariantsInterface001*>(message.GetApiFunction(1));
    return g_DynamicArmorVariantsInterface;
}
