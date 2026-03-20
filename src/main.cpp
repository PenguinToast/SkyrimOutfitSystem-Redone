#include "EquipmentCatalog.h"
#include "Hooks.h"
#include "InputManager.h"
#include "Serialization.h"
#include "integrations/DynamicArmorVariantsExtendedClient.h"

static void SKSEMessageHandler(SKSE::MessagingInterface::Message *a_message) {
  switch (a_message->type) {
  case SKSE::MessagingInterface::kPostLoad:
    sosr::integrations::DynamicArmorVariantsExtendedClient::Refresh();
    break;
  case SKSE::MessagingInterface::kDataLoaded:
    sosr::EquipmentCatalog::Get().RefreshFromGame();
    logger::info("Equipment catalog initialized");
    sosr::InputManager::GetSingleton()->Init();
    logger::info("Input manager initialized");
    break;
  case SKSE::MessagingInterface::kPostPostLoad:
    sosr::hooks::Install();
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

  messaging->RegisterListener("SKSE", SKSEMessageHandler);

  if (auto *serialization = SKSE::GetSerializationInterface()) {
    serialization->SetUniqueID(sosr::serialization::kID);
    serialization->SetSaveCallback(&sosr::serialization::SaveCallback);
    serialization->SetLoadCallback(&sosr::serialization::LoadCallback);
    serialization->SetRevertCallback(&sosr::serialization::RevertCallback);
  }

  logger::info("Skyrim Outfit System Redone loaded");
  return true;
}
