#pragma once

#include <vector>
namespace korka {
  namespace parser_node {
    struct expression {

    };
  }

  class parser {

    enum class node_type {
      program,
      function_decl, var_decl,
      compound_stmt, if_stmt, while_stmt, return_stmt,
      binary_expr,  literal, identifier
    };

    struct node {

    };

  public:

  private:
    std::vector<node> m_nodes;
  };
}