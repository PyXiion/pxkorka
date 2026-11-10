#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <span>
#include <vector>

namespace korka {
  class byte_writer {
  public:
    constexpr byte_writer() = default;

    /**
     * Appends the value in host byte order, returns the position it was written at.
     */
    constexpr auto write(std::integral auto const &v) -> std::size_t {
      return write(std::bit_cast<std::array<std::byte, sizeof(v)>>(v));
    }

    constexpr auto write(std::span<const std::byte> data) -> std::size_t {
      auto i = m_data.size();
      std::ranges::copy(data, std::back_inserter(m_data));
      return i;
    }

    /**
     * Overwrites already written bytes starting at `pos`.
     */
    constexpr auto patch(std::size_t pos, std::integral auto const &v) -> void {
      auto bytes = std::bit_cast<std::array<std::byte, sizeof(v)>>(v);
      std::ranges::copy(bytes, m_data.begin() + static_cast<std::ptrdiff_t>(pos));
    }

    constexpr auto size() const -> std::size_t {
      return m_data.size();
    }

    constexpr auto data() -> std::vector<std::byte> & {
      return m_data;
    }

  private:
    std::vector<std::byte> m_data;
  };
}
