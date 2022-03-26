# Clox Specifications

## Parsing technique used
Operator precedence (bottom up parser) parsing is used for evaluating expression
Recursive descent parsing used for syntax

## CLox Grammar
```
    declaration ->  varDecl
                |   statement ;
                
    statement   ->  exprStmt
                |   printStmt 
                |   block ;

    block       -> "{" declaration* "}" ;
```