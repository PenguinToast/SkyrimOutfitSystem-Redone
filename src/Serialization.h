#pragma once

#include <SKSE/SKSE.h>

namespace sosr::serialization {
constexpr std::uint32_t kID = 'SOSR';

void SaveCallback(SKSE::SerializationInterface *a_skse);
void LoadCallback(SKSE::SerializationInterface *a_skse);
void RevertCallback(SKSE::SerializationInterface *a_skse);
} // namespace sosr::serialization
