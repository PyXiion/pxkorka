#pragma once

#include "frozen/bits/elsa.h"
#include "korka/shared/error.hpp"
#include "korka/shared/flat_map.hpp"
#include "korka/utils/overloaded.hpp"
#include "korka/vm/op_codes.hpp"
#include "parser.hpp"
#include "korka/vm/bytecode_builder.hpp"
#include "korka/utils/frozen_hash_string_view.hpp"
#include <ranges>
#include <vector>
#include <optional>
#include <string_view>

namespace korka {
  struct void_t {
  };

  using vm::type_info;

  constexpr auto string_to_type(std::string_view name) -> type {
    if (name == "int") return type::i64;
    else if (name == "void") return type::void_;
    // TODO: other types
    return type::i64;
  }

  constexpr auto type_to_string(type t) -> std::string_view {
    switch (t) {
      case type::void_:
        return "void";
      case type::i64:
        return "int";
    }
  }

  struct variable_info {
    std::string_view name;
    type_info type;

    std::size_t locals_index;

    static constexpr auto from_node(const nodes::decl_var &node) -> variable_info {
      return {
        .name = node.var_name,
        .type{},
        .locals_index{}
      };
    }
  };

  struct function_info {
    std::string_view name;
    std::vector<variable_info> params;
    type_info return_type;

    vm::bytecode_builder::label label;
  };

  template<std::size_t NMaxParams>
  struct const_function_info {
    std::string_view name;
    std::size_t param_count;
    std::array<variable_info, NMaxParams> params;
    type_info return_type;

    vm::bytecode_builder::label label;
  };

  template<auto info_getter>
  struct _extract_function_signature {
    template<typename IndexSequence>
    struct param_helper;

    template<std::size_t... Is>
    struct param_helper<std::index_sequence<Is...>> {
      // Map each parameter info to its corresponding C++ type
      using type = vm::type_info_to_cpp_t<[]{return info_getter().return_type;}>(
        vm::type_info_to_cpp_t<[]{return info_getter().params[Is].type;}>...
      );
    };

    using type = typename param_helper<
      std::make_index_sequence<info_getter().param_count>
    >::type;
  };

  template<auto info_getter>
  using const_function_info_to_signature_t = typename _extract_function_signature<info_getter>::type;

  template<std::size_t NMaxParams>
  constexpr auto function_info_to_const(const function_info &f) {
    const_function_info<NMaxParams> info{
      .name = f.name,
      .param_count = f.params.size(),
      .params{},
      .return_type = f.return_type,
      .label{}
    };

    std::ranges::copy(f.params, std::begin(info.params));
    return info;
  }

  struct symbol_table {
    struct scope {
      flat_map<std::string_view, variable_info> variables;

      std::size_t current_locals_size{};
    };
    std::vector<scope> scopes;
    flat_map<std::string_view, function_info> functions;

    constexpr auto push_scope() -> void { scopes.emplace_back(); }

    constexpr auto pop_scope() -> void { scopes.pop_back(); }

    constexpr auto declare_var(std::string_view name, const type_info &type) -> std::expected<variable_info, error_t> {
      if (scopes.empty()) {
        return std::unexpected{error::other_compiler_error{
          .message = "No scope"
        }};
      }

      auto &current = scopes.back();
      if (current.variables.contains(name)) {
        return std::unexpected{error::redeclaration{
          .identifier = name
        }};
      }

      variable_info info{
        .name = name,
        .type = type,
        .locals_index = current.current_locals_size++
      };

      current.variables[name] = info;
      return info;
    }

    constexpr auto declare_function(std::string_view name, auto &&...args) -> std::expected<void, error_t> {
      functions.emplace(std::piecewise_construct,
                        std::forward_as_tuple(name),
                        std::forward_as_tuple(name, std::forward<decltype(args)>(args)...));
      return {};
    }

    constexpr auto lookup_variable(std::string_view name) -> std::optional<variable_info> {
      for (auto &scope: std::ranges::reverse_view(scopes)) {
        if (auto var_it = scope.variables.find(name); var_it != std::end(scope.variables)) {
          return var_it->second;
        }
      }
      return std::nullopt;
    }

    constexpr auto lookup_function(std::string_view name) -> std::optional<function_info> {
      if (auto func_it = functions.find(name); func_it != std::end(functions)) {
        return func_it->second;
      }
      return std::nullopt;
    }

    constexpr auto clear() -> void {
      scopes.clear();
      functions.clear();
    }
  };


  struct compilation_result {
    std::vector<std::byte> bytes;
    flat_map<std::string_view, function_info> functions;
  };

  template<std::size_t NBytes, std::size_t NFunctions, std::size_t NMaxParams, class SignatureMapper>
  struct const_compilation_result {
    std::array<std::byte, NBytes> bytes;
    frozen::unordered_map<std::string_view, const_function_info<NMaxParams>, NFunctions> functions;

    template<const_string name>
    using get_signature_t = typename SignatureMapper::template get_signature_t<name>;

    template<const_string name>
    constexpr auto function() const -> get_signature_t<name> * {
      return nullptr;
    }

    SignatureMapper mapper{};
  };

template<std::size_t>
struct unique_type{};

  template<auto function_info_getter, typename IndexSequence>
  struct signature_mapper;

  template<auto function_info_getter, std::size_t... Is>
  struct signature_mapper<function_info_getter, std::index_sequence<Is...>> {
    constexpr static auto _overloaded = overloaded{
      ([](unique_type<frozen::elsa<std::string_view>{}(function_info_getter(Is).name, 0)>)
        -> const_function_info_to_signature_t<[] {return function_info_getter(Is);}> *
      {
        return nullptr;
      })...
    };

    template<const_string name>
    using get_signature_t = std::remove_pointer_t<decltype(_overloaded(unique_type<frozen::elsa<std::string_view>{}(name, 0)>{}))>;

    std::tuple<const_function_info_to_signature_t<[] {return function_info_getter(Is);}> *...> debug1;
    std::tuple<unique_type<frozen::elsa<std::string_view>{}(function_info_getter(Is).name, 0)>...> debug2;
  };


  template<auto r>
  constexpr auto compilation_result_to_const() {
    // --- BYTES ---
    constexpr static auto bytes = to_array<[] { return r().bytes; }>();

    // --- FUNCTIONS ---
    constexpr static auto function_count = []() constexpr {
      return r().functions.size();
    }();
    constexpr static auto max_params_n = []() constexpr {
      std::size_t n{};
      for (auto &&f: r().functions) {
        n = std::max(n, f.second.params.size());
      }
      return n;
    }();
    constexpr static auto functions = []() constexpr {
      std::array<std::pair<std::string_view, const_function_info<max_params_n>>, function_count> functions_data{};
      std::size_t i{};
      for (auto &&[key, value]: r().functions) {
        functions_data[i++] = (std::make_pair(key, function_info_to_const<max_params_n>(value)));
      }
      return frozen::make_unordered_map(functions_data);
    };

    using sign_mapper = signature_mapper<[](std::size_t i) { return (functions().begin() + i)->second; }, std::make_index_sequence<function_count>>;

    return const_compilation_result<bytes.size(), function_count, max_params_n, sign_mapper>{
      bytes,
      functions()
    };
  }

  class compiler {
  public:
    constexpr compiler(std::span<const nodes::node> nodes, nodes::index_t root_node)
      : m_nodes(nodes), m_root_node(root_node) {}

    constexpr auto compile() -> std::expected<compilation_result, error_t> {
      m_symbols.push_scope();
      auto ok = process_node(m_root_node);
      if (!ok) return std::unexpected{ok.error()};

      return compilation_result{
        builder.build(),
        m_symbols.functions
      };
    }

  private:
    std::span<const nodes::node> m_nodes;
    nodes::index_t m_root_node;
    symbol_table m_symbols;
    vm::bytecode_builder builder;

    // Info for ast walker
    std::optional<type_info> m_current_func_ret;

    using result_t = std::expected<type_info, error_t>;

    constexpr auto process_node(nodes::index_t idx) -> result_t {
      const auto &node = m_nodes[idx];

      return std::visit(overloaded{
        [&](const nodes::decl_program &program) -> result_t {
          for (auto item: nodes::get_list_view(m_nodes, program.external_declarations_head)) {
            auto ok = process_node(item);
            if (not ok) {
              return std::unexpected{ok.error()};
            }
          }

          return type_info{type::void_};
        },
        [&](const nodes::decl_function &function) -> result_t {
          type_info ret_type = string_to_type(function.ret_type);
          m_current_func_ret = ret_type;

          // Function pointer
          auto label = builder.make_label();

          // Collect parameter info for the function signature
          std::vector<variable_info> parameters;
          for (auto p_idx: nodes::get_list_view(m_nodes, function.params_head)) {
            const auto &p_node = std::get<nodes::decl_var>(m_nodes[p_idx].data);
            parameters.push_back({
                                   .name = p_node.var_name,
                                   .type = string_to_type(p_node.type_name),
                                   .locals_index = 0
                                 });
          }

          // Register the function globally BEFORE processing the body
          // This allows the function to "see" itself (recursion)
          auto reg_ok = m_symbols.declare_function(
            function.name,
            parameters,
            ret_type,
            label
          );
          if (not reg_ok) return std::unexpected{reg_ok.error()};

          // Entering function scope
          m_symbols.push_scope();
          m_current_func_ret = ret_type;
          builder.bind_label(label);

          // Handle parameters as local variables
          for (auto &&param: parameters) {
            auto ok = m_symbols.declare_var(param.name, param.type);
            if (not ok) {
              return std::unexpected{ok.error()};
            }
          }

          // Function body
          for (auto stmt: nodes::get_list_view(m_nodes, function.body)) {
            if (auto res = process_node(stmt); !res) {
              m_symbols.pop_scope(); // clean up
              return res;
            }
          }

          // cleanup
          m_symbols.pop_scope();
          m_current_func_ret.reset();

          return {};
        },

        [&](const nodes::expr_literal &lit) -> result_t {
          return std::visit([&](auto val) -> result_t {
            using T = decltype(val);
            if constexpr (std::is_same_v<T, std::int64_t>) {
              builder.emit_const<type::i64>(val);
              return type_info{type::i64};
            }
            return std::unexpected{
              error::other_compiler_error{.message = "This type is not supported as a literal yet"}};
          }, lit);
        },
        [&](const nodes::stmt_block &block) -> result_t {
          for (auto stmt: nodes::get_list_view(m_nodes, block.children_head)) {
            if (auto res = process_node(stmt); !res) return res;
          }
          return {};
        },

        [&](const nodes::stmt_return &stmt) -> result_t {
          auto actual_type = process_node(stmt.expr);
          if (!actual_type) return actual_type;

          // Semantic Check: Does return type match function signature?
          if (m_current_func_ret && *actual_type != *m_current_func_ret) {
            // TODO: proper error
            return std::unexpected{error::other_compiler_error{
              .message = "Function return type mismatch"
            }};
          }

          builder.emit_op(vm::op_code::ret);
          return *actual_type;
        },
        [&](const nodes::decl_var &var) -> result_t {
          auto ok = m_symbols.declare_var(var.var_name, string_to_type(var.type_name));
          if (!ok) {
            return std::unexpected{ok.error()};
          }

          if (var.init_expr != nodes::empty_node) {
            auto expr = process_node(var.init_expr);
            if (not expr) {
              return expr;
            }
            builder.emit_save_local(ok->locals_index);
          }
          return ok->type;
        },

        [&](const nodes::expr_var &var) -> result_t {
          auto info = m_symbols.lookup_variable(var.name);
          if (!info) {
            return std::unexpected{error::undefined_symbol{
              .identifier = var.name
            }};
          }

          builder.emit_load_local(info->locals_index);
          return info->type;
        },
        [&](const nodes::expr_binary &expr) -> result_t {
          auto left = process_node(expr.left);
          if (not left) {
            return left;
          }
          auto right = process_node(expr.right);
          if (not right) {
            return right;
          }

          if ((*left) != (*right)) {
            return std::unexpected{error::other_compiler_error{
              "Expected same types in the binary expression"
            }};
          }

          auto code = vm::get_op_code_for_math(*left, *right, expr.op);
          if (not code) {
            return std::unexpected{code.error()};
          }
          builder.emit_op(*code);

          return *left;
        },

        [&](const nodes::stmt_if &if_) -> result_t {
          auto condition_expr = process_node(if_.condition);
          if (not condition_expr) {
            return condition_expr;
          }

          auto else_branch_label = builder.make_label();
          auto end_label = builder.make_label();

          if (if_.else_branch == nodes::empty_node) {
            builder.emit_jmp_if_zero(end_label);

            auto then_branch = process_node(if_.then_branch);
            if (not then_branch) {
              return then_branch;
            }
          } else {
            builder.emit_jmp_if_zero(else_branch_label);

            auto then_branch = process_node(if_.then_branch);
            if (not then_branch) {
              return then_branch;
            }

            builder.emit_jmp(end_label);

            builder.bind_label(else_branch_label);
            auto else_branch = process_node(if_.else_branch);
            if (not else_branch) {
              return else_branch;
            }
          }

          builder.bind_label(end_label);
          return {};
        },

        [&](const auto &value) -> result_t {
          std::ignore = value;
          return std::unexpected{error::other_compiler_error{
            "Not implemented"
          }};
        }
      }, node.data);
    }
  };

  template<auto &&nodes, nodes::index_t root>
  constexpr static auto compile_nodes() {
    constexpr static auto expected = [] constexpr {
      return compiler{nodes, root}.compile();
    };

    if constexpr (not expected()) {
      report_error<[]{return expected.error();}>();
    } else {
      return compilation_result_to_const<[] constexpr { return expected().value(); }>();
    }
  }

  template<const_string code>
  consteval static auto compile() {
    constexpr static auto nodes_root = parse<code>();

    return compile_nodes<nodes_root.first, nodes_root.second>();
  }
} // namespace korka