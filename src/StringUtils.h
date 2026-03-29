#pragma once

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>
#include <string_view>

namespace sosr::strings {
inline std::string TrimText(std::string_view a_text) {
  std::size_t start = 0;
  while (start < a_text.size() &&
         std::isspace(static_cast<unsigned char>(a_text[start])) != 0) {
    ++start;
  }

  std::size_t end = a_text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(a_text[end - 1])) != 0) {
    --end;
  }

  return std::string(a_text.substr(start, end - start));
}

inline int CompareTextInsensitive(std::string_view a_left,
                                  std::string_view a_right) {
  const auto leftSize = a_left.size();
  const auto rightSize = a_right.size();
  const auto count = (std::min)(leftSize, rightSize);

  for (std::size_t index = 0; index < count; ++index) {
    const auto left = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_left[index])));
    const auto right = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_right[index])));
    if (left < right) {
      return -1;
    }
    if (left > right) {
      return 1;
    }
  }

  if (leftSize < rightSize) {
    return -1;
  }
  if (leftSize > rightSize) {
    return 1;
  }
  return 0;
}

inline bool EqualsInsensitive(std::string_view a_left,
                              std::string_view a_right) {
  return a_left.size() == a_right.size() &&
         std::ranges::equal(
             a_left, a_right, [](const char a_lhs, const char a_rhs) {
               return std::tolower(static_cast<unsigned char>(a_lhs)) ==
                      std::tolower(static_cast<unsigned char>(a_rhs));
             });
}

inline bool StartsWithInsensitive(std::string_view a_text,
                                  std::string_view a_prefix) {
  return a_text.size() >= a_prefix.size() &&
         CompareTextInsensitive(a_text.substr(0, a_prefix.size()), a_prefix) ==
             0;
}

inline bool ContainsInsensitive(std::string_view a_text,
                                std::string_view a_pattern) {
  if (a_pattern.empty()) {
    return true;
  }

  if (a_pattern.size() > a_text.size()) {
    return false;
  }

  for (std::size_t start = 0; start + a_pattern.size() <= a_text.size();
       ++start) {
    if (CompareTextInsensitive(a_text.substr(start, a_pattern.size()),
                               a_pattern) == 0) {
      return true;
    }
  }

  return false;
}
} // namespace sosr::strings
