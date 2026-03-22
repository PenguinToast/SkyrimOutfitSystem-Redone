#pragma once

#ifndef SVS_VERSION_MAJOR
#define SVS_VERSION_MAJOR 1
#endif

#ifndef SVS_VERSION_MINOR
#define SVS_VERSION_MINOR 1
#endif

#ifndef SVS_VERSION_PATCH
#define SVS_VERSION_PATCH 1
#endif

#ifndef SVS_VERSION_STRING
#define SVS_VERSION_STRING "1.1.1-dev"
#endif

namespace Plugin {
inline constexpr auto NAME = "Skyrim Vanity System"sv;
inline constexpr REL::Version VERSION{SVS_VERSION_MAJOR, SVS_VERSION_MINOR,
                                      SVS_VERSION_PATCH, 0};
inline constexpr std::string_view VERSION_STRING{SVS_VERSION_STRING};
inline constexpr auto AUTHOR = "PenguinToast"sv;
} // namespace Plugin
