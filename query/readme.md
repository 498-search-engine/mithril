
Potential query language 

query         ::= clause (WS clause)* ;

clause        ::= modifier? expression boost? ;

modifier      ::= "+" | "-" ;

boost         ::= "^" number ;

expression    ::= scoped_expr | grouped_expr | term_expr ;

scoped_expr   ::= field ":" (regex_expr | phrase_expr | range_expr | term_expr) ;

grouped_expr  ::= "(" query ")" ;

term_expr     ::= word ;

phrase_expr   ::= QUOTE phrase QUOTE ;

regex_expr    ::= "/" regex "/" ;

range_expr    ::= range_operator value ;

range_operator ::= ">" | ">=" | "<" | "<=" ;

field         ::= identifier ;

word          ::= escaped_word ;

phrase        ::= (escaped_word | WS)* ;

regex         ::= (escaped_regex_char | any_char_except_slash)+ ;

value         ::= number | quoted_date ;

quoted_date   ::= QUOTE date_string QUOTE ;

escaped_word         ::= (ESCAPED_CHAR | NON_SPECIAL_CHAR)+ ;
escaped_regex_char   ::= ESCAPED_CHAR ;
any_char_except_slash ::= any_char - "/" ;

identifier    ::= (ALPHA | DIGIT | "_")+ ;

number        ::= DIGIT+ ("." DIGIT+)? ;

date_string   ::= DIGIT DIGIT DIGIT DIGIT "-" DIGIT DIGIT "-" DIGIT DIGIT ;

ESCAPED_CHAR        ::= "\\" SPECIAL_CHAR ;
SPECIAL_CHAR        ::= "+" | "-" | "=" | "&" | "|" | ">" | "<" | "!" | "(" | ")" | "{" | "}" | "[" | "]" | "^" | "\"" | "~" | "*" | "?" | ":" | "\\" | "/" | WS ;
NON_SPECIAL_CHAR    ::= any_char - SPECIAL_CHAR ;
QUOTE               ::= "\"" ;
WS                  ::= " " | "\t" | "\n" | "\r" ;
ALPHA               ::= "a".."z" | "A".."Z" ;
DIGIT               ::= "0".."9" ;
