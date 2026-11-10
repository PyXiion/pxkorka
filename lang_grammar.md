```
program                 ::= { external_declaration } ;
external_declaration    ::= function_definition | struct_declaration ;

struct_declaration      ::= "struct" identifier "{" field_declaration { field_declaration } "}" ";" ;
field_declaration       ::= type_specifier identifier ";" ;

function_definition     ::= type_specifier identifier "(" [ parameter_list ] ")" compound_stmt ;

parameter_list          ::= param_decl { "," param_decl } ;
param_decl              ::= type_specifier identifier ;

type_specifier          ::= "int" | "void" | identifier ;

statement               ::= compound_stmt
                          | expression_stmt
                          | if_stmt
                          | while_stmt
                          | return_stmt ;

compound_stmt           ::= "{" { declaration_in_block | statement } "}" ;
declaration_in_block    ::= type_specifier identifier [ "=" expression ] ";" ;

expression_stmt         ::= expression ";" | ";" ;
if_stmt                 ::= "if" "(" expression ")" statement [ "else" statement ] ;
while_stmt              ::= "while" "(" expression ")" statement ;
return_stmt             ::= "return" [ expression ] ";" ;


expression              ::= assignment ;
assignment              ::= logical_or [ "=" assignment ] ;

logical_or              ::= logical_and { "or" logical_and } ;

logical_and             ::= equality { "and" equality } ;

equality                ::= relational { ("==" | "!=") relational } ;

relational              ::= additive { ("<" | ">" | "<=" | ">=") additive } ;

additive                ::= multiplicative { ("+" | "-") multiplicative } ;

multiplicative          ::= unary { ("*" | "/" | "%") unary } ;

unary                   ::= ("-" | "!" | "+") unary
                          | postfix ;

postfix                 ::= primary { "." identifier } ;

primary                 ::= identifier
                          | number
                          | "(" expression ")"
                          | func_call ;

func_call               ::= identifier "(" [ argument_list ] ")" ;
argument_list           ::= expression { "," expression } ;
```