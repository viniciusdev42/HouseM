#include <iostream>
#include <string>
#include <vector>

#include "src/Main/Model/Structures/String/Tokenization/Tokenizer.hpp"

using Tokenizer::Tokenizer;
using Tokenizer::Text::PunctuationMode;
using Tokenizer::Text::TokenizerOptions;

using Tokens = std::vector<std::string>;

namespace {

int failures = 0;
int total = 0;

std::string show(const Tokens& tokens) {
    std::string out = "{";
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) out += ", ";
        out += "\"" + tokens[i] + "\"";
    }
    return out + "}";
}

void check(const std::string& name, const Tokens& actual, const Tokens& expected) {
    ++total;
    if (actual == expected) {
        std::cout << "[ OK ] " << name << "\n";
        return;
    }
    ++failures;
    std::cout << "[FAIL] " << name << "\n"
              << "   esperado: " << show(expected) << "\n"
              << "   obtido:   " << show(actual) << "\n";
}

Tokens run(const std::string& text, TokenizerOptions options = TokenizerOptions{}) {
    return Tokenizer(options).tokenize(text);
}

TokenizerOptions withMode(PunctuationMode mode) {
    TokenizerOptions options;
    options.punctuation = mode;
    return options;
}

}  // namespace

int main() {
    /* --- Casos básicos --- */
    check("texto vazio gera nenhum token", run(""), {});

    check("separa por espacos, ignorando espacos repetidos e bordas",
          run("  um   dois\tum\n"), {"um", "dois", "um"});

    /* --- Modos de pontuação --- */
    check("Separate: pontuacao vira token proprio",
          run("Olá, mundo! Guarda-chuva."),
          {"Olá", ",", "mundo", "!", "Guarda-chuva", "."});

    check("Discard: pontuacao e removida",
          run("oi, mundo!", withMode(PunctuationMode::Discard)), {"oi", "mundo"});

    check("Keep: pontuacao fica na palavra",
          run("oi, mundo!", withMode(PunctuationMode::Keep)), {"oi,", "mundo!"});

    /* --- Conectores (hifen e apostrofo) --- */
    check("mantem hifen e apostrofo entre letras",
          run("guarda-chuva d'água"), {"guarda-chuva", "d'água"});

    check("hifen no fim da palavra e separado",
          run("oi- mundo"), {"oi", "-", "mundo"});

    TokenizerOptions noConnectors;
    noConnectors.keepIntraWordConnectors = false;
    check("keepIntraWordConnectors=false separa o hifen",
          run("guarda-chuva", noConnectors), {"guarda", "-", "chuva"});

    /* --- Delimitadores customizados --- */
    TokenizerOptions delimiters;
    delimiters.delimiters = U",;";
    check("delimitadores customizados sao descartados",
          run("a,b;c", delimiters), {"a", "b", "c"});

    check("delimitadores consecutivos nao geram tokens vazios por padrao",
          run("a,,b", delimiters), {"a", "b"});

    delimiters.removeEmptyTokens = false;
    check("removeEmptyTokens=false gera token vazio (estilo split)",
          run("a,,b", delimiters), {"a", "", "b"});

    /* --- UTF-8 --- */
    check("letras acentuadas fazem parte da palavra",
          run("açaí café"), {"açaí", "café"});

    /* Literais divididos para o escape hexadecimal nao consumir a letra seguinte. */
    check("NBSP (U+00A0) funciona como espaco",
          run("a\xC2\xA0" "b"), {"a", "b"});

    check("byte UTF-8 invalido e preservado como simbolo",
          run("a\xFF" "b"), {"a", "\xFF", "b"});

    std::cout << "\n" << (total - failures) << "/" << total << " testes passaram\n";
    return failures == 0 ? 0 : 1;
}
