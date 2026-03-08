#pragma once
#include <cstdint>

namespace korka {
  enum class type {
    void_,
    i64
  };

  namespace detail {
    template<type>
    struct type_to_cpp_;
    template<>
    struct type_to_cpp_<type::void_> {
      using type = void;
    };

    template<>
    struct type_to_cpp_<type::i64> {
      using type = std::int64_t;
    };
  }

  template<type T>
  using type_to_cpp_t = typename detail::type_to_cpp_<T>::type;
}