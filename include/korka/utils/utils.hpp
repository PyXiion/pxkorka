#pragma once

#include <array>
#include <vector>
#include <algorithm>

namespace korka {
  template<auto data_functor>
  constexpr auto to_array() {
    constexpr static std::size_t size = data_functor().size();

    std::array<typename decltype(data_functor())::value_type, size> out;
    auto in = data_functor();
    for (int i = 0; i < size; ++i) {
      out[i] = in[i];
    }
    return out;
  }
}