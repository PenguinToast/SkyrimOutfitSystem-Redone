#include "Hooks.h"
#include "InputManager.h"

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* a_message)
{
    switch (a_message->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        sosng::InputManager::GetSingleton()->Init();
        logger::info("Input manager initialized");
        break;
    case SKSE::MessagingInterface::kPostPostLoad:
        sosng::hooks::Install();
        break;
    default:
        break;
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    REL::Module::reset();

    auto* messaging = reinterpret_cast<SKSE::MessagingInterface*>(
        a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));

    if (!messaging) {
        logger::critical("Failed to load messaging interface. This is fatal.");
        return false;
    }

    SKSE::Init(a_skse);
    SKSE::AllocTrampoline(1 << 10);

    messaging->RegisterListener("SKSE", SKSEMessageHandler);

    logger::info("Skyrim Outfit System NG loaded");
    return true;
}
