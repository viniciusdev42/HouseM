#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "src/Main/Model/Structures/String/Core/View/StringView.hpp"

namespace Tokenizer {

using TokenizerView = StringView::StringView;

namespace Text {

/**
 * Define como caracteres classificados como pontuação/símbolo são tratados
 * quando não fazem parte de um conector intra-palavra.
 */
enum class PunctuationMode {
    Separate,  // Vira token próprio: "oi!" -> "oi", "!"
    Discard,   // É descartada:      "oi!" -> "oi"
    Keep       // Permanece no token: "oi!" -> "oi!"
};

/**
 * Opções de configuração do tokenizador UTF-8.
 */
struct TokenizerOptions {
    // Delimitadores explícitos (têm prioridade sobre pontuação).
    std::u32string delimiters;

    // Estratégia padrão para pontuação/símbolos.
    PunctuationMode punctuation = PunctuationMode::Separate;

    // Quando false, delimitadores consecutivos produzem tokens vazios.
    bool removeEmptyTokens = true;

    // Mantém conectores (ex.: hífen e apóstrofo) entre caracteres de palavra,
    // exceto quando punctuation == PunctuationMode::Keep (onde tudo já fica).
    bool keepIntraWordConnectors = true;
};

}  // namespace Text

/**
 * Tokenizador de texto UTF-8 que preserva a ordem original dos tokens.
 *
 * Prioridade de classificação de cada code point:
 *   1. Delimitador customizado -> encerra token atual e é descartado
 *   2. Espaço Unicode          -> encerra token atual e é descartado
 *   3. Pontuação / símbolo    -> tratado conforme PunctuationMode
 *   4. Palavra                 -> anexado ao token atual
 */
class Tokenizer {
public:
    using Options = Text::TokenizerOptions;
    using PunctuationMode = Text::PunctuationMode;

    explicit Tokenizer(Options options = Options{});

    /**
     * Divide `text` em tokens na ordem de ocorrência.
     * Não lança exceções; bytes UTF-8 inválidos são preservados byte a byte.
     */
    std::vector<std::string> tokenize(TokenizerView text) const;

private:
    enum class CharClass {
        Delimiter,
        Whitespace,
        Word,
        Punctuation
    };

    CharClass classify(char32_t codePoint) const;

    bool shouldStayInsideToken(char32_t codePoint,
                               bool previousIsWordChar,
                               TokenizerView text,
                               std::size_t nextPos) const;

    void flushToken(std::vector<std::string>& tokens, std::string& current) const;

    Options options_;
};

}  // namespace Tokenizer

#endif  // TOKENIZER_HPP