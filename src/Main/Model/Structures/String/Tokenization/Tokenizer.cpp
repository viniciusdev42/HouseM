#include "Tokenizer.hpp"

#include <utility>

namespace {

struct DecodedCodePoint {
    char32_t value = 0xFFFD;
    std::size_t length = 0;  // Quantidade de bytes consumidos em `text`.
};

/**
 * Decodifica um code point UTF-8 a partir de `pos`.
 * Em caso de sequência inválida (truncada, overlong, surrogate ou > U+10FFFF),
 * retorna U+FFFD com length = 1 para avançar 1 byte e preservar o dado bruto.
 */
DecodedCodePoint decodeUtf8At(Tokenizer::TokenizerView text, std::size_t pos) {
    if (pos >= text.size()) {
        return {0xFFFD, 0};
    }

    const DecodedCodePoint invalid{0xFFFD, 1};
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    const unsigned char lead = bytes[pos];

    if (lead < 0x80) {
        return {static_cast<char32_t>(lead), 1};
    }

    std::size_t length = 0;
    char32_t value = 0;
    char32_t minimum = 0;  // Menor code point válido para o tamanho da sequência.

    if ((lead & 0xE0) == 0xC0) {
        length = 2;
        value = lead & 0x1F;
        minimum = 0x80;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
        value = lead & 0x0F;
        minimum = 0x800;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
        value = lead & 0x07;
        minimum = 0x10000;
    } else {
        return invalid;
    }

    if (pos + length > text.size()) {
        return invalid;
    }

    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char continuation = bytes[pos + i];
        if ((continuation & 0xC0) != 0x80) {
            return invalid;
        }
        value = (value << 6) | static_cast<char32_t>(continuation & 0x3F);
    }

    const bool isSurrogate = (value >= 0xD800 && value <= 0xDFFF);
    if (value < minimum || value > 0x10FFFF || isSurrogate) {
        return invalid;
    }

    return {value, length};
}

// Espaços ASCII, NBSP e espaços tipográficos comuns em textos latinos.
bool isWhitespace(char32_t cp) {
    return (cp >= 0x09 && cp <= 0x0D) ||
           cp == 0x20 ||
           cp == 0xA0 ||
           (cp >= 0x2000 && cp <= 0x200B) ||
           cp == 0x202F;
}

// Letras latinas, ordinais (ª/º), marcas combinantes (NFD) e dígitos ASCII.
bool isWordChar(char32_t cp) {
    if ((cp >= U'a' && cp <= U'z') ||
        (cp >= U'A' && cp <= U'Z') ||
        (cp >= U'0' && cp <= U'9')) {
        return true;
    }

    if (cp == 0xAA || cp == 0xBA) {
        return true;
    }
    if (cp >= 0xC0 && cp <= 0x24F) {
        return cp != 0xD7 && cp != 0xF7;  // Exclui × e ÷.
    }
    if (cp >= 0x0300 && cp <= 0x036F) {
        return true;
    }
    return cp >= 0x1E00 && cp <= 0x1EFF;
}

// Hífen ASCII, apóstrofo ASCII e apóstrofo tipográfico (’).
bool isConnector(char32_t cp) {
    return cp == U'-' || cp == U'\'' || cp == 0x2019;
}

void appendRaw(std::string& target, Tokenizer::TokenizerView raw) {
    target.append(raw.data(), raw.size());
}

}  // namespace

namespace Tokenizer {

Tokenizer::Tokenizer(Options options) : options_(std::move(options)) {}

std::vector<std::string> Tokenizer::tokenize(TokenizerView text) const {
    std::vector<std::string> tokens;
    if (text.size() == 0) {
        return tokens;
    }

    std::string current;
    // Indica que há um token aberto pronto para ser finalizado ao encontrar
    // um separador/fim; fica falso após pontuação isolada para não gerar
    // tokens vazios espúrios logo após sinais como "." ou ",".
    bool tokenOpen = true;
    bool previousIsWordChar = false;

    std::size_t pos = 0;
    while (pos < text.size()) {
        const DecodedCodePoint decoded = decodeUtf8At(text, pos);
        const TokenizerView raw = text.substr(pos, decoded.length);
        const std::size_t nextPos = pos + decoded.length;

        switch (classify(decoded.value)) {
            case CharClass::Delimiter:
            case CharClass::Whitespace:
                if (tokenOpen) {
                    flushToken(tokens, current);
                }
                tokenOpen = true;
                previousIsWordChar = false;
                break;

            case CharClass::Word:
                appendRaw(current, raw);
                tokenOpen = true;
                previousIsWordChar = true;
                break;

            case CharClass::Punctuation:
                if (shouldStayInsideToken(decoded.value, previousIsWordChar, text, nextPos)) {
                    appendRaw(current, raw);
                    tokenOpen = true;
                    previousIsWordChar = false;
                    break;
                }

                if (!current.empty()) {
                    flushToken(tokens, current);
                }
                if (options_.punctuation == PunctuationMode::Separate) {
                    tokens.emplace_back(raw.data(), raw.size());
                }
                tokenOpen = false;
                previousIsWordChar = false;
                break;
        }

        pos = nextPos;
    }

    if (tokenOpen) {
        flushToken(tokens, current);
    }

    return tokens;
}

Tokenizer::CharClass Tokenizer::classify(char32_t codePoint) const {
    if (options_.delimiters.find(codePoint) != std::u32string::npos) {
        return CharClass::Delimiter;
    }
    if (isWhitespace(codePoint)) {
        return CharClass::Whitespace;
    }
    if (isWordChar(codePoint)) {
        return CharClass::Word;
    }
    return CharClass::Punctuation;
}

bool Tokenizer::shouldStayInsideToken(char32_t codePoint,
                                    bool previousIsWordChar,
                                    TokenizerView text,
                                    std::size_t nextPos) const {
    if (options_.punctuation == PunctuationMode::Keep) {
        return true;
    }
    if (!options_.keepIntraWordConnectors || !previousIsWordChar || !isConnector(codePoint)) {
        return false;
    }
    if (nextPos >= text.size()) {
        return false;
    }

    const DecodedCodePoint next = decodeUtf8At(text, nextPos);
    return classify(next.value) == CharClass::Word;
}

void Tokenizer::flushToken(std::vector<std::string>& tokens, std::string& current) const {
    if (!current.empty() || !options_.removeEmptyTokens) {
        tokens.push_back(std::move(current));
    }
    current.clear();
}

}  // namespace Tokenizer