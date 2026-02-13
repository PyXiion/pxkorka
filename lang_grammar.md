```
program                 ::= { external_declaration } ;
external_declaration    ::= function_definition | global_declaration ;

function_definition     ::= type_specifier identifier "(" [ parameter_list ] ")" compound_stmt ;
global_declaration      ::= type_specifier init_declarator_list ";" ;

parameter_list          ::= param_decl { "," param_decl } ;
param_decl              ::= type_specifier identifier ;

type_specifier          ::= "int" | "char" | "void" ;

statement               ::= compound_stmt 
                          | expression_stmt 
                          | if_stmt 
                          | while_stmt 
                          | return_stmt ;

compound_stmt           ::= "{" { declaration_in_block } { statement } "}" ;
declaration_in_block    ::= type_specifier init_declarator_list ";" ;

expression_stmt         ::= [ expression ] ";" ;
if_stmt                 ::= "if" "(" expression ")" statement [ "else" statement ] ;
while_stmt              ::= "while" "(" expression ")" statement ;
return_stmt             ::= "return" [ expression ] ";" ;

init_declarator_list    ::= init_declarator { "," init_declarator } ;
init_declarator         ::= identifier [ "=" expression ] ;


expression              ::= assignment ;
assignment              ::= identifier "=" assignment 
                          | logical_or ;

logical_or              ::= logical_and { "or" logical_and } ;

logical_and             ::= equality { "and" equality } ;

equality                ::= relational { ("==" | "!=") relational } ;

relational              ::= additive { ("<" | ">" | "<=" | ">=") additive } ;

additive                ::= multiplicative { ("+" | "-") multiplicative } ;

multiplicative          ::= unary { ("*" | "/" | "%") unary } ;

unary                   ::= ("-" | "!" | "+") unary 
                          | primary ;

primary                 ::= identifier 
                          | number 
                          | "(" expression ")" 
                          | func_call ;

func_call               ::= identifier "(" [ argument_list ] ")" ;
argument_list           ::= expression { "," expression } ;
```