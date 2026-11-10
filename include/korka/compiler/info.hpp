#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>
#include "korka/shared/types.hpp"
#include "korka/vm/bytecode_builder.hpp"

namespace korka {
  struct function_info {
    std::string_view name;
    std::vector<type_info> param_types;
    std::size_t param_slots{};
    type_info return_type;
    std::size_t return_slots{};

    vm::bytecode_builder::label label{};
    std::size_t start_pos{};
  };

  template<std::size_t NMaxParams>
  struct const_function_info {
    std::string_view name{};
    std::size_t param_count{};
    std::array<type_info, NMaxParams> param_types{};
    type_info return_type{};

    std::size_t start_pos{};
  };
}
