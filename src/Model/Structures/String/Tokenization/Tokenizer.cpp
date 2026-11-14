#include "Tokenizer.hpp"

using TV =TokenizerView;

/* --------- Funções auxiliares ------------ */

namespace {

struct DecodedCodePoint {
    char32_t value;
    std::size_t length;  /* bytes consumidos no texto original */
};

/* Decodifica um code point UTF-8 em `pos`. Sequência inválida (truncada,
   overlong, surrogate ou > U+10FFFF) consome 1 byte e retorna U+FFFD, o que
   garante progresso e permite copiar os bytes originais para o token.*/
DecodedCodePoint decodeUtf8At(TV s, std::size_t pos) {
    const DecodedCodePoint invalid{0xFFFD, 1};
    const char* bytes = s.data();

    const unsigned char lead = static_cast<unsigned char>(bytes[pos]);
    if (lead < 0x80) return {lead, 1};

    std::size_t length = 0;
    char32_t value = 0;
    char32_t minimum = 0;  /* menor valor legítimo para esse tamanho (anti-overlong)*/

    if ((lead & 0xE0) == 0xC0)      { length = 2; value = lead & 0x1F; minimum = 0x80; }
    else if ((lead & 0xF0) == 0xE0) { length = 3; value = lead & 0x0F; minimum = 0x800; }
    else if ((lead & 0xF8) == 0xF0) { length = 4; value = lead & 0x07; minimum = 0x10000; }
    else return invalid;

    if (pos + length > s.size()) return invalid;

    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char byte = static_cast<unsigned char>(bytes[pos + i]);
        if ((byte & 0xC0) != 0x80) return invalid;
        value = (value << 6) | (byte & 0x3F);
    }

    const bool isSurrogate = value >= 0xD800 && value <= 0xDFFF;
    if (value < minimum || value > 0x10FFFF || isSurrogate) return invalid;

    return {value, length};
}


/* ASCII, NBSP e os espaços tipográficos comuns em textos latinos
  (U+2000–U+200B, U+202F).*/
bool isWhitespace(char32_t cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || cp == 0xA0 ||
           (cp >= 0x2000 && cp <= 0x200B) || cp == 0x202F;
}

/* Letras latinas (ASCII, Latin-1, Latin Extended-A/B e Additional), os
 indicadores ordinais ª/º, acentos combinantes (texto em NFD) e dígitos ASCII.*/
bool isWordChar(char32_t cp) {
    if (cp >= U'a' && cp <= U'z') return true;
    if (cp >= U'A' && cp <= U'Z') return true;
    if (cp >= U'0' && cp <= U'9') return true;

    if (cp == 0xAA || cp == 0xBA) return true;
    if (cp >= 0xC0 && cp <= 0x24F) return cp != 0xD7 && cp != 0xF7;  /* exclui × e ÷ */
    if (cp >= 0x0300 && cp <= 0x036F) return true;
    return cp >= 0x1E00 && cp <= 0x1EFF;
}

/* Hífen, apóstrofo ASCII e apóstrofo tipográfico (U+2019).*/
bool isConnector(char32_t cp) {
    return cp == U'-' || cp == U'\'' || cp == 0x2019;
}

} // namespace

/* ---------- Tokenizer ------------- */ 

Tokenizer::Tokenizer(TokenizerOptions options) : options_(std::move(options)) {}

std::vector<std::string> Tokenizer::tokenize(TV text) const {
    std::vector<std::string> tokens;
    if (text.size() == 0) return tokens;

    std::string current;
    /* "Aberto" = existe um token em construção (mesmo que ainda vazio). Fecha
       após pontuação, pois ela já encerrou o token anterior; isso evita tokens
     vazios espúrios logo depois de "." ou ",". */
    bool tokenOpen = true;
    bool lastWasWordChar = false;

    std::size_t pos = 0;
    while (pos < text.size()) {
        const DecodedCodePoint decoded = decodeUtf8At(text, pos);
        const TV raw = text.substr(pos, decoded.length);
        const std::size_t nextPos = pos + decoded.length;

        switch (classify(decoded.value)) {
            case CharClass::Delimiter:
            case CharClass::Whitespace:
                if (tokenOpen) flush(tokens, current);
                tokenOpen = true;
                lastWasWordChar = false;
                break;

            case CharClass::Word:
                current.append(raw.data(), raw.size());
                tokenOpen = true;
                lastWasWordChar = true;
                break;

            case CharClass::Punctuation:
                if (shouldStayInsideToken(decoded.value, lastWasWordChar, text, nextPos)) {
                    current.append(raw.data(), raw.size());
                    tokenOpen = true;
                    lastWasWordChar = false;
                    break;
                }
                if (!current.empty()) flush(tokens, current);
                if (options_.punctuation == PunctuationMode::Separate) {
                    tokens.emplace_back(raw.data(), raw.size());
                }
                tokenOpen = false;
                lastWasWordChar = false;
                break;
        }
        pos = nextPos;
    }

    if (tokenOpen) flush(tokens, current);
    return tokens;
}

 Tokenizer::CharClass Tokenizer::classify(char32_t codePoint) const {
    if (options_.delimiters.find(codePoint) != std::u32string::npos) return CharClass::Delimiter;
    if (isWhitespace(codePoint)) return CharClass::Whitespace;
    if (isWordChar(codePoint)) return CharClass::Word;
    return CharClass::Punctuation;
}

/* Em modo Keep a pontuação sempre fica no token. Nos demais modos, só
   hífen/apóstrofo cercados por letras ou dígitos permanecem no token.*/
bool Tokenizer::shouldStayInsideToken(char32_t codePoint, bool lastWasWordChar,
                                      TV text, std::size_t nextPos) const {
    if (options_.punctuation == PunctuationMode::Keep) return true;
    if (!options_.keepIntraWordConnectors) return false;
    if (!lastWasWordChar || !isConnector(codePoint)) return false;
    if (nextPos >= text.size()) return false;

    const DecodedCodePoint next = decodeUtf8At(text, nextPos);
    return classify(next.value) == CharClass::Word;
}

void Tokenizer::flush(std::vector<std::string>& tokens, std::string& current) const {
    if (!current.empty() || !options_.removeEmptyTokens) {
        tokens.push_back(std::move(current));
    }
    current.clear();
}
