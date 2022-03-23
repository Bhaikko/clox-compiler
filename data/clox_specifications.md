# Clox Specifications

## Parsing technique used
Operator precedence (bottom up parser) parsing is used

## CLox Grammar
```
    declaration ->  varDecl
                |   statement ;
                
    statement   ->  exprStmt
                |   printStmt 
                |   block ;

    block       -> "{" declaration* "}" ;
```