#pragma once
#include <cstddef>

namespace korka::vm {
  /**
   * A script function that can be given to `context::call`.
   * `Signature` is the C++ signature of the function, e.g. `std::int64_t(std::int64_t)`.
   */
  template<class Signature>
  struct function_handle {
    using signature_t = Signature;

    std::size_t start_pos;
  };
}
