grammar Lucix;

application:
    (variableDeclaration | functionDeclaration)* ;

variableDeclaration:
    ID ':' type ('=' expression)? ';' ;

functionDeclaration:
    ID '(' functionParameters? ')' ('->' type)? block ;

type: 'i32' ;

functionParameters:
    parameter (',' parameter)* ;

parameter:
    ID ':' type ;

expression
    : ID '(' expressionList? ')'            # CallExpr
    | '-' expression                        # MinusExpr
    | expression op=('*' | '/') expression  # MulDivExpr
    | expression op=('+' | '-') expression  # AddSubExpr
    | expression op=('<' | '>') expression  # LeGeExpr
    | ID                                    # IdExpr
    | INT                                   # IntExpr
    | '(' expression ')'                    # ParensExpr
    | assign                                # AssignExpr
    ;

expressionList:
    expression (',' expression)* ;

block:
    '{' statement* '}' ;

statement
    : variableDeclaration
    | ifStatement
    | block
    | assign ';'
    | jumpExpression
    | expression   ';' ;

ifStatement
 : 'if' expression block ('else' ifStatement? block)?
 ;

assign:
    ID '=' expression ;

jumpExpression:
    'return' expression? ';' ;

ID:
    Letter (Letter | Digit)* ;

INT:
    Digit+ ;

fragment Digit:
    [0-9] ;

fragment Letter:
    [a-zA-Z] ;

SPACE:
    [ \t\n\r]+ -> skip ;

COMMENT:
    ';;' ~[\r\n]* -> skip ;
