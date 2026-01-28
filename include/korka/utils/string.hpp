#pragma once

#include <cstddef>
#include <algorithm>
#include <cstring>

namespace korka {
  template<std::size_t N>
  struct const_string {
    constexpr const_string() = default;

    constexpr const_string(const char (&str)[N]) {
      std::copy_n(str, N, value);
    }

    constexpr operator std::string_view() const {
      return {value, value + N - 1};
    }

    char value[N]{};
    const std::size_t length = N;


  };

  template<typename T>
  concept StringLiteral = requires(std::decay_t<T> t) {
    { t.value } -> std::convertible_to<const char*>;
    { t.length } -> std::convertible_to<std::size_t>;
  };

  consteval bool operator ==(StringLiteral auto const &l, StringLiteral auto const &r)  {
    if (l.length != r.length) return false;
    return std::equal(l.value, l.value + l.length, r.value);
  }

  consteval bool operator ==(const char *l, StringLiteral auto const &r)  {
    if (strlen(l) != r.length) return false;
    return std::equal(l, l + r.length, r.value);
  }

  consteval bool operator ==(StringLiteral auto const &l, const char *r)  {
    if (strlen(r) != l.length) return false;
    return std::equal(l.value, l.value + l.length, r);
  }

  constexpr bool operator +(StringLiteral auto l, StringLiteral auto r) {
    const_string<l.length + r.length> result;
    auto it = std::copy_n(std::begin(l.value), l.length, std::begin(result.value));
    std::copy_n(std::begin(r.value), r.length, it);
    return result;
  }

}