#include "EquipmentCatalog.h"
#include "Hooks.h"
#include "InputManager.h"
#include "Plugin.h"
#include "Serialization.h"
#include "integrations/DynamicArmorVariantsExtendedClient.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/Menu.h"

static void SKSEMessageHandler(SKSE::MessagingInterface::Message *a_message) {
  switch (a_message->type) {
  case SKSE::MessagingInterface::kDataLoaded:
    sosr::Menu::GetSingleton()->SetGameDataLoaded(true);
    break;
  case SKSE::MessagingInterface::kPostPostLoad:
    sosr::integrations::DynamicArmorVariantsExtendedClient::Refresh();
    sosr::hooks::Install();
    break;
  case SKSE::MessagingInterface::kPreLoadGame:
  case SKSE::MessagingInterface::kPostLoadGame:
    sosr::ui::conditions::ConditionParamOptionCache::Get().Reset();
    break;
  default:
    break;
  }
}

extern "C" DLLEXPORT bool SKSEAPI
SKSEPlugin_Load(const SKSE::LoadInterface *a_skse) {
  REL::Module::reset();

  auto *messaging = reinterpret_cast<SKSE::MessagingInterface *>(
      a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));

  if (!messaging) {
    logger::critical("Failed to load messaging interface. This is fatal.");
    return false;
  }

  SKSE::Init(a_skse);
  SKSE::AllocTrampoline(1 << 10);
  logger::info("{} build {}", Plugin::NAME, Plugin::VERSION_STRING);
  sosr::integrations::DynamicArmorVariantsExtendedClient::Initialize(a_skse);

  messaging->RegisterListener("SKSE", SKSEMessageHandler);

  if (auto *serialization = SKSE::GetSerializationInterface()) {
    serialization->SetUniqueID(sosr::serialization::kID);
    serialization->SetSaveCallback(&sosr::serialization::SaveCallback);
    serialization->SetLoadCallback(&sosr::serialization::LoadCallback);
    serialization->SetRevertCallback(&sosr::serialization::RevertCallback);
  }

  logger::info("{} loaded", Plugin::NAME);
  return true;
}
