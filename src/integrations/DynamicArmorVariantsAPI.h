#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace DynamicArmorVariantsAPI
{
    constexpr auto DynamicArmorVariantsPluginName = "DynamicArmorVariants";

    struct DynamicArmorVariantsMessage
    {
        enum : std::uint32_t
        {
            kMessage_QueryInterface = 'DAVI'
        };

        void* (*GetApiFunction)(unsigned int revisionNumber) = nullptr;
    };

    struct IDynamicArmorVariantsInterface001;
    IDynamicArmorVariantsInterface001* GetDynamicArmorVariantsInterface001();

    struct IDynamicArmorVariantsInterface001
    {
        virtual bool IsReady() = 0;
        virtual bool RegisterVariantJson(const char* a_name, const char* a_variantJson) = 0;
        virtual bool DeleteVariant(const char* a_name) = 0;
        virtual bool SetVariantConditionsJson(const char* a_name, const char* a_conditionsJson) = 0;
    };
}

extern DynamicArmorVariantsAPI::IDynamicArmorVariantsInterface001* g_DynamicArmorVariantsInterface;
