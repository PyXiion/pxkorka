//
// Created by pyxiion on 09.03.2026.
//

#pragma once

#include "korka/vm/op_codes.hpp"
#include <span>
#include <stdexcept>

namespace korka::vm {
  template<class T>
  concept byte_readable = std::integral<T> or std::is_enum_v<T>;

  class byte_reader {
  public:
    explicit byte_reader(std::span<const std::byte> bytes, std::size_t start_pos = 0)
      : m_cursor(start_pos), m_bytes(bytes) {

    }

    template<byte_readable T>
    constexpr auto read() -> T {
      std::array<std::byte, sizeof(T)> b;
      read(b);
      return std::bit_cast<T>(b);
    }

    constexpr auto read(std::span<std::byte> out) -> void {
      if (m_cursor + out.size() > m_bytes.size()) {
        throw std::out_of_range("byte_reader::read: out of range");
      }

      std::copy(std::begin(m_bytes) + m_cursor, std::begin(m_bytes) + m_cursor + static_cast<long long>(out.size()), std::begin(out));
      m_cursor += out.size();
    }

    auto cursor() const noexcept -> std::size_t {
      return m_cursor;
    }

    auto set_cursor(std::size_t pos) noexcept -> void {
      m_cursor = pos;
    }

  private:
    std::size_t m_cursor;
    const std::span<const std::byte> m_bytes;
  };
}