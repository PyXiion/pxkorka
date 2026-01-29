#pragma once
#include <concepts>
#include <vector>
#include <span>
#include <cstdint>
#include <ranges>

namespace korka {
  class byte_writer {
  public:
    constexpr byte_writer() = default;

    constexpr auto write(std::integral auto const &v) -> std::size_t {
      return write(std::bit_cast<std::array<std::byte, sizeof(v)>>(v));
    }

    template<std::size_t bytes>
    constexpr auto write(std::int64_t v) -> std::size_t {
      auto bytes_arr = std::bit_cast<std::array<std::byte, sizeof(v)>>(v);
      auto cut_bytes = std::array<std::byte, bytes>();
      for (std::size_t i = 0; i < bytes; ++i)
        cut_bytes[i] = bytes_arr[i];
      return write(cut_bytes);
    }

    constexpr auto write(std::span<const std::byte> data) -> std::size_t {
      auto i = m_data.size();
      std::ranges::copy(data, std::back_inserter(m_data));
      return i;
    }

    constexpr auto write_many(auto &&...data) -> void {
      (write(std::forward<decltype(data)>(data)), ...);
    }

    constexpr auto data() -> std::vector<std::byte> & {
      return m_data;
    }

  private:
    std::vector<std::byte> m_data;
  };
}