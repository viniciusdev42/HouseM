/* Tokenizador de texto UTF-8, header-only (C++17), sem dependências externas.
***
*   Contrato do módulo: recebe um texto e devolve uma coleção de tokens,
*   na mesma ordem em que aparecem no texto.
***
*  Prioridade de classificação de cada code point:
*     1. delimitador customizado  -> encerra o token, é descartado
*     2. espaço (Unicode)         -> encerra o token, é descartado
*     3. pontuação/símbolo        -> tratado conforme PunctuationMode
*     4. qualquer outro           -> faz parte da palavra
***
*   Exemplo:
**     text::Tokenizer tokenizer;
**     auto tokens = tokenizer.tokenize("Olá, mundo! Guarda-chuva.");
****** {"Olá", ",", "mundo", "!", "Guarda-chuva", "."}
*/
#ifndef TOKENIZER
#define TOKENIZER

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Text {

enum class PunctuationMode {
    Separate,  // vira token próprio: "oi!" -> "oi", "!"
    Discard,   // é removida:         "oi!" -> "oi"
    Keep       // fica na palavra:    "oi!" -> "oi!"
};

struct TokenizerOptions {
    /* Delimitadores extras, como code points (ex.: U",;|"). Têm prioridade
     sobre a regra de pontuação.*/
    std::u32string delimiters;

    PunctuationMode punctuation = PunctuationMode::Separate;

    /* Quando false, delimitadores consecutivos geram tokens vazios (como em
      um "split"): "a,,b" com ',' como delimitador -> "a", "", "b". */
    bool removeEmptyTokens = true;

    /* Mantém hífen e apóstrofo entre duas letras ("guarda-chuva", "d'água").
     Ignorado em PunctuationMode::Keep. */
    bool keepIntraWordConnectors = true;
};

}  // namespace Text

class Tokenizer {
public:
    explicit Tokenizer(TokenizerOptions options = TokenizerOptions{});

    /* Devolve os tokens na ordem original. Nunca lança exceção: bytes UTF-8
       inválidos são tratados como símbolos e preservados byte a byte.*/
    std::vector<std::string> tokenize(std::string_view text) const;

private:
    enum class CharClass { Delimiter, Whitespace, Word, Punctuation };

    CharClass classify(char32_t codePoint) const;
    
    bool shouldStayInsideToken(char32_t codePoint, bool lastWasWordChar,
                               std::string_view text, std::size_t nextPos) const;
                               
    void flush(std::vector<std::string>& tokens, std::string& current) const;

    TokenizerOptions options_;                                 
};
#endif
