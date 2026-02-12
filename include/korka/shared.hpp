#pragma once
#include <variant>
#include <string_view>
#include <cstdint>

namespace korka {
  using literal_value_t = std::variant<std::monostate, std::string_view, std::int64_t, double>;
}