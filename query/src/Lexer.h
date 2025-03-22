/*
 * Lexer.h
 *
 * Declaration of a stream of tokens that you can read from.
 *
 * You do not have to modify this file, but you may choose to do so.

 */

#ifndef LEXER_H_
#define LEXER_H_

#include <string>
#include <utility>


#include "token.h"

class Lexer {
public:
    explicit Lexer(const std::string& input);

    /**
     * Returns the next token in the stream.
     * If at end of input, returns TokenType::EOF_TOKEN.
     */
    Token NextToken();

    /**
     * Returns the next token without consuming it.
     */
    Token PeekToken();

    /**
     * Checks if the input has been fully consumed.
     */
    bool EndOfInput() const;

private:
    std::string input;
    size_t position = 0;
    Token peekedToken;
    bool hasPeeked = false;

    // Core lexing helpers
    void SkipWhitespace();
    char PeekChar() const;
    char GetChar();
    bool MatchChar(char expected);
    bool IsAlpha(char c) const;
    bool IsAlnum(char c) const;
    bool IsOperatorKeyword(const std::string& word) const;
    bool IsFieldKeyword(const std::string& word) const;

    // Token lexing functions
    Token LexWordOrKeyword();      // Handles WORD, OPERATOR, FIELD
    Token LexQuotedPhrase();       // Handles PHRASE
    Token LexSymbol();             // Handles COLON, LPAREN, RPAREN
};

#endif // LEXER_H_











// // *-----

// // #include "expression.h"



// /**
//  * Check if a character is relevant to a query expression
//  * Relevant characters are defined as
//  * +
//  * -
//  * *
//  * (
//  * )
//  *
//  * and all digits
//  */
// bool CharIsRelevant(char c){
    
// }

// /**
//     * Opposite of char is relevant. Needed for filtering the input.
//     */
// bool CharIsIrrelevant(char c){
    
// }

// /**
//  * The token stream, which you can both Match( ) a single character from,
//  * or ParseNumber( ) to consume a whole int64_t.
//  *
//  * The input string by default is filtered of any characters that are
//  * deemed "irrelevant" by the CharIsIrrelevant function above.
//  */
// class TokenStream {
//     // The input we receive, with only relevant characters left
//     std::string input;
//     // Where we currently are in the input
//     size_t location = 0;

// public:
//     /**
//      * Construct a token stream that uses a copy of the input
//      * that contains only characters relevant to math expressions
//      */
//     TokenStream(const std::string& in);

//     /**
//      * Attempt to match and consume a specific character
//      *
//      * Returns true if the char was matched and consumed, false otherwise
//      */
//     bool Match(char c);

//     /**
//      * Check whether all the input was consumed
//      */
//     bool AllConsumed() const;

//     /**
//      * Attempt to match and consume a whole number
//      *
//      * Return a dynamically allocated Number if successful, nullptr otherwise
//      */
//     SimpleTerm* ParseSimpleTerm();
// };

#endif /* TOKENSTREAM_H_ */