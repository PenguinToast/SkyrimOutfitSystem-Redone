#include "PrismaUI_API.h"
#include <keyhandler/keyhandler.h>

namespace
{
    constexpr auto kViewPath = "skyrim-outfit-system-ng/index.html";
    constexpr auto kToggleVisibilityKey = 0x3D; // F3

    PRISMA_UI_API::IVPrismaUI1* g_prismaUI{ nullptr };
    PrismaView g_view{};

    void HideView();

    void OnViewDomReady(PrismaView a_view)
    {
        logger::info("PrismaUI view is ready: {}", a_view);
        g_prismaUI->Invoke(a_view, "updateFocusLabel('Hidden. Press F3 to open the outfit system UI.')");
    }

    void OnDebugMessageFromUI(const char* a_data)
    {
        logger::info("Received data from UI: {}", a_data);
    }

    void OnHideViewRequest([[maybe_unused]] const char* a_data)
    {
        if (!g_prismaUI || !g_prismaUI->IsValid(g_view) || g_prismaUI->IsHidden(g_view)) {
            return;
        }

        HideView();
    }

    void ShowView()
    {
        g_prismaUI->Show(g_view);

        if (g_prismaUI->Focus(g_view)) {
            g_prismaUI->Invoke(g_view, "updateFocusLabel('Visible. Press F3 to hide the outfit system UI.')");
        }
    }

    void HideView()
    {
        if (g_prismaUI->HasFocus(g_view)) {
            g_prismaUI->Unfocus(g_view);
        }

        g_prismaUI->Invoke(g_view, "updateFocusLabel('Hidden. Press F3 to open the outfit system UI.')");
        g_prismaUI->Hide(g_view);
    }
}

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* a_message)
{
    if (a_message->type != SKSE::MessagingInterface::kDataLoaded) {
        return;
    }

    g_prismaUI = static_cast<PRISMA_UI_API::IVPrismaUI1*>(
        PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1));

    if (!g_prismaUI) {
        logger::critical("Failed to acquire the PrismaUI API. Make sure PrismaUI is installed.");
        return;
    }

    g_view = g_prismaUI->CreateView(kViewPath, OnViewDomReady);
    g_prismaUI->RegisterJSListener(g_view, "sendDebugMessageToSKSE", OnDebugMessageFromUI);
    g_prismaUI->RegisterJSListener(g_view, "hideOutfitSystemUI", OnHideViewRequest);
    g_prismaUI->Hide(g_view);

    KeyHandler::RegisterSink();
    auto* keyHandler = KeyHandler::GetSingleton();
    [[maybe_unused]] const auto toggleVisibilityHandler = keyHandler->Register(kToggleVisibilityKey, KeyEventType::KEY_DOWN, []() {
        if (!g_prismaUI || !g_prismaUI->IsValid(g_view)) {
            logger::warn("PrismaUI view is not available.");
            return;
        }

        if (g_prismaUI->IsHidden(g_view)) {
            ShowView();
            return;
        }

        HideView();
    });
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
