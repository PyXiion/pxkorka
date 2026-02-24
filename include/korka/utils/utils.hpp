#pragma once

#include <array>
#include <vector>
#include <algorithm>

namespace korka {
  template<auto data_getter>
  constexpr auto to_array() {
    using value_type = typename decltype(data_getter())::value_type;
    constexpr static std::size_t size = data_getter().size();

    std::array<value_type, size> out;
    auto in = data_getter();
    for (std::size_t i = 0; i < size; ++i) {
      out[i] = in[i];
    }
    return out;
  }
}