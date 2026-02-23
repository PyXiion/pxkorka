#pragma once

#include <array>
#include <string>
#include <string_view>
#include <charconv>
#include <tuple>
#include <type_traits>

namespace korka {
  namespace detail {
    constexpr auto to_string_helper(const auto &value) -> std::string {
      using T = std::decay_t<decltype(value)>;

      if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::string{static_cast<std::string_view>(value)};
      } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, char>) {
        return std::string(1, value);
      } else if constexpr (requires { std::to_chars(std::declval<char *>(), std::declval<char *>(), value); }) {
        std::array<char, 64> buffer{};
        auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        return std::string(buffer.data(), ptr);
      } else {
        return "?";
      }
    }
  }

  constexpr auto format(std::string_view fmt, auto &&... args) -> std::string {
    std::string result;
    result.reserve(fmt.length() + (sizeof...(args) * 8));

    auto args_tuple = std::make_tuple(detail::to_string_helper(args)...);

    size_t arg_index = 0;
    for (size_t i = 0; i < fmt.length(); ++i) {
      if (fmt[i] == '~' && arg_index < sizeof...(args)) {
        std::apply([&](const auto &... unpacked_args) {
          size_t current = 0;
          ((current++ == arg_index ? (void) (result += unpacked_args) : (void) 0), ...);
        }, args_tuple);

        arg_index++;
      } else {
        result += fmt[i];
      }
    }
    return result;
  }

  // Тесты в compile-time
  static_assert(format("Hello ~!", 123) == "Hello 123!");
  static_assert(format("~ + ~ = ~", 1, 1, 2) == "1 + 1 = 2");
  static_assert(format("Name: ~", "Korka") == "Name: Korka");
}