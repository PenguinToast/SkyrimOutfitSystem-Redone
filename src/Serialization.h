#pragma once

#include <SKSE/SKSE.h>

namespace sosng::serialization {
constexpr std::uint32_t kID = 'SOSN';

void SaveCallback(SKSE::SerializationInterface *a_skse);
void LoadCallback(SKSE::SerializationInterface *a_skse);
void RevertCallback(SKSE::SerializationInterface *a_skse);
} // namespace sosng::serialization
