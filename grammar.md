# Stella Language Grammar

## Formal Grammar

program         = { declaration } EOF ;

declaration     = pubDeclaration
| fnDeclaration
| classDeclaration
| varDeclaration
| statement ;

pubDeclaration  = "pub" ( fnDeclaration | classDeclaration | varDeclaration ) ;

fnDeclaration   = "fn" identifier function ;

anonymousFunction = "fn" function ;

function        = "(" parameters ")" block ;

parameters      = [ identifier { "," identifier } ] ;

classDeclaration= "class" identifier [ "<" identifier ] "{" { method } "}" ;

method          = "fn" identifier function ;

varDeclaration  = "let" identifier { "," identifier } [ "=" expression { "," expression } ] ";" ;

statement       = block
| ifStatement
| whileStatement
| forStatement
| returnStatement
| useStatement
| matchStatement
| expressionStatement ;

block           = "{" { declaration } "}" ;

ifStatement     = "if" expression statement [ "else" statement ] ;

whileStatement  = "while" expression statement ;

forStatement    = "for" ( varDeclaration | expressionStatement | ";" ) expression? ";" expression? statement ;

returnStatement = "return" [ expression { "," expression } ] ";" ;

useStatement    = "use" [ "(" ] importNameList [ ")" ] "from" string ";" ;

importNameList  = importName { "," importName } ;

importName      = identifier [ "as" identifier ] ;

matchStatement  = "match" expression "{" { matchArm } "}" ;

matchArm        = ( "default" | "Ok" [ "(" identifier ")" ] | "Err" [ "(" identifier ")" ] | expression ) "=>" ( block | expression ";" ) ;

expressionStatement = expression ";" ;

expression      = assignment ;

assignment      = ( logic_or "=" assignment )
| ( logic_or compound_assignment logic_or )
| logic_or ;

compound_assignment = "+=" | "-=" | "*=" | "/=" ;

logic_or        = logic_and { "or" logic_and } ;

logic_and       = equality { "and" equality } ;

equality        = comparison { ( "!=" | "==" ) comparison } ;

comparison      = shift { ( ">" | ">=" | "<" | "<=" ) shift } ;

shift           = term { ( "<<" | ">>" ) term } ;

term            = factor { ( "-" | "+" ) factor } ;

factor          = unary { ( "/" | "*" | "%" ) unary } ;

unary           = ( "!" | "-" ) unary
| call ;

call            = primary { ( "(" arguments ")" ) | ( "." identifier [ "(" arguments ")" ] ) | ( "[" expression "]" ) } ;

primary         = "true" | "false" | "nil"
| number
| string
| identifier
| "self"
| "super" "." identifier
| anonymousFunction
| arrayLiteral
| tableLiteral
| "(" expression ")" ;

arguments       = [ expression { "," expression } ] ;

arrayLiteral    = "[" [ expression { "," expression } ] "]" ;

tableLiteral    = "{" [ tableEntry { "," tableEntry } ] "}" ;

tableEntry      = expression ":" expression ;

number          = NUMBER ;

string          = STRING ;
identifier      = IDENTIFIER ;

### Recognized tokens
EOF, NUMBER, STRING, IDENTIFIER,
"let", "fn", "class", "if", "else", "while", "for", "return", "use", "from", "pub", "true", "false", "nil", "super", "self", "match", "default", "Ok", "Err", "as"
"(", ")", "{", "}", "[", "]", ",", ".", ";", "/", "*", "%", "+", "-", "!", "=", "==", "!=", ">", ">=", "<", "<=", "<<", ">>", "=>", "or", "and" , "+=", "-=", "*=", "/=", 
" " ", " ' " 

### Notation 
This grammar is written in Extended Backus-Naur Form (EBNF).

= defines a rule.

| separates alternatives.

[ ... ] denotes optional elements.

{ ... } denotes zero or more repetitions.

( ... ) groups elements.

Terminals (tokens) are in "double quotes" or UPPERCASE (like INT, IDENTIFIER, EOF).

Non-terminals are in lowercase (like program, declaration, expression).
