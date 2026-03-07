#pragma once

#include <frozen/bits/elsa.h>
#include <string_view>

namespace frozen {
  template<>
  struct elsa<std::string_view> {
    constexpr std::size_t operator()(std::string_view const &value, std::size_t seed) const {
      auto hash = elsa<char>{};

      for (auto &&c : value) {
        auto h = hash(c, seed);
        seed ^= h;
        seed += h;
      }

      return seed;
    }
  };
}
