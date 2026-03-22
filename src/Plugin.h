#pragma once

#ifndef SOSR_VERSION_MAJOR
#define SOSR_VERSION_MAJOR 1
#endif

#ifndef SOSR_VERSION_MINOR
#define SOSR_VERSION_MINOR 1
#endif

#ifndef SOSR_VERSION_PATCH
#define SOSR_VERSION_PATCH 0
#endif

#ifndef SOSR_VERSION_STRING
#define SOSR_VERSION_STRING "1.1.0-dev"
#endif

namespace Plugin {
inline constexpr auto NAME = "Skyrim Outfit System Redone"sv;
inline constexpr REL::Version VERSION{SOSR_VERSION_MAJOR, SOSR_VERSION_MINOR,
                                      SOSR_VERSION_PATCH, 0};
inline constexpr std::string_view VERSION_STRING{SOSR_VERSION_STRING};
inline constexpr auto AUTHOR = "PenguinToast"sv;
} // namespace Plugin
