grammar SysY2023;

compUnit: decl* funcDef* mainFuncDef;

decl: constDecl | varDecl;

constDecl: CONSTTK bType constDef (COMMA constDef)* SEMICN;
bType: INTTK;
constDef: IDENFR (LBRACK constExp RBRACK)* ASSIGN constInitVal;
constInitVal: constExp | LBRACE constInitVal (COMMA constInitVal)* RBRACE;

varDecl: bType varDef (COMMA varDef)* SEMICN;
varDef: IDENFR (LBRACK constExp RBRACK)* (ASSIGN initVal)?;
initVal: exp | LBRACE initVal (COMMA initVal)* RBRACE;

funcDef: funcType IDENFR LPARENT funcFParams? RPARENT block;
mainFuncDef: INTTK MAINTK LPARENT RPARENT block;
funcType: INTTK | VOIDTK;
funcFParams: funcFParam (COMMA funcFParam)*;
funcFParam: bType IDENFR (LBRACK RBRACK (LBRACK constExp RBRACK)*)?;

block: LBRACE blockItem* RBRACE;
blockItem: decl | stmt;

stmt: lVal ASSIGN exp SEMICN
    | exp? SEMICN
    | block
    | IFTK LPARENT cond RPARENT stmt (ELSETK stmt)?
    | FORTK LPARENT forStmt? SEMICN cond? SEMICN forStmt? RPARENT stmt
    | BREAKTK SEMICN
    | CONTINUETK SEMICN
    | RETURNTK exp? SEMICN
    | lVal ASSIGN GETINTTK LPARENT RPARENT SEMICN
    | PRINTFTK LPARENT STRCON (COMMA exp)* RPARENT SEMICN
    ;
forStmt: lVal ASSIGN exp;
exp: addExp;
cond: lOrExp;
lVal: IDENFR (LBRACK exp RBRACK)*;
primaryExp: LPARENT exp RPARENT | lVal | number;
number: INTCON;
unaryExp: primaryExp | IDENFR LPARENT funcRParams? RPARENT | unaryOp unaryExp;
unaryOp: PLUS | MINU | NOT;
funcRParams: exp (COMMA exp)*;
mulExp: unaryExp ((MULT | DIV | MOD) unaryExp)*;
addExp: mulExp ((PLUS | MINU) mulExp)*;
relExp: addExp ((LSS | LEQ | GRE | GEQ) addExp)*;
eqExp: relExp ((EQL | NEQ) relExp)*;
lAndExp: eqExp (AND eqExp)*;
lOrExp: lAndExp (OR lAndExp)*;
constExp: addExp;

// literal
INTCON: '0' | [1-9][0-9]*;
STRCON: '"' .*? '"';

// keywords
MAINTK: 'main';
CONSTTK: 'const';
INTTK: 'int';
VOIDTK: 'void';
BREAKTK: 'break';
CONTINUETK: 'continue';
IFTK: 'if';
ELSETK: 'else';
FORTK: 'for';
GETINTTK: 'getint';
PRINTFTK: 'printf';
RETURNTK: 'return';

// operators
NOT: '!';
AND: '&&';
OR: '||';
PLUS: '+';
MINU: '-';
MULT: '*';
DIV: '/';
MOD: '%';
LSS: '<';
LEQ: '<=';
GRE: '>';
GEQ: '>=';
EQL: '==';
NEQ: '!=';
ASSIGN: '=';
SEMICN: ';';
COMMA: ',';

// parentheses
LPARENT: '(';
RPARENT: ')';
LBRACK: '[';
RBRACK: ']';
LBRACE: '{';
RBRACE: '}';

// identifier
IDENFR: [a-zA-Z_][a-zA-Z0-9_]*;

// whitespaces and comments
WS : [ \r\n\t]+ -> skip;
LineComment : '//' ~ [\r\n]* -> skip;
BlockCommit : '/*' .*? '*/' -> skip;
