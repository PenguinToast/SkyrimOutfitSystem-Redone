#include "Hooks.h"

#include "InputManager.h"
#include "Menu.h"

namespace
{
    static void hk_PollInputDevices(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent** a_events);
    static inline REL::Relocation<decltype(hk_PollInputDevices)> g_inputHandler;

    void hk_PollInputDevices(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent** a_events)
    {
        if (a_events) {
            sosng::InputManager::GetSingleton()->AddEventToQueue(a_events);
        }

        if (sosng::Menu::GetSingleton()->IsEnabled()) {
            static RE::InputEvent* dummy[] = { nullptr };
            g_inputHandler(a_dispatcher, dummy);
        } else {
            g_inputHandler(a_dispatcher, a_events);
        }
    }
}

namespace sosng::hooks
{
    struct D3DInitHook
    {
        static void thunk()
        {
            func();

            auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
            auto* context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);
            auto* swapChain = reinterpret_cast<IDXGISwapChain*>(renderer->GetRuntimeData().renderWindows->swapChain);
            auto* device = reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder);

            Menu::GetSingleton()->Init(swapChain, device, context);
        }

        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct PresentHook
    {
        static void thunk(std::uint32_t a_argument)
        {
            func(a_argument);
            Menu::GetSingleton()->Draw();
        }

        static inline REL::Relocation<decltype(thunk)> func;
    };

    void Install()
    {
        auto& trampoline = SKSE::GetTrampoline();

        logger::info("Hooking BSInputDeviceManager::PollInputDevices");
        g_inputHandler = trampoline.write_call<5>(
            REL::RelocationID(67315, 68617).address() + REL::Relocate(0x7B, 0x7B, 0x81),
            hk_PollInputDevices);

        logger::info("Hooking BSGraphics::Renderer::InitD3D");
        D3DInitHook::func = trampoline.write_call<5>(
            REL::RelocationID(75595, 77226).address() + REL::Relocate(0x50, 0x2BC),
            D3DInitHook::thunk);

        logger::info("Hooking DXGI present");
        PresentHook::func = trampoline.write_call<5>(
            REL::RelocationID(75461, 77246).address() + REL::VariantOffset(0x9, 0x9, 0x15).offset(),
            PresentHook::thunk);
    }
}
