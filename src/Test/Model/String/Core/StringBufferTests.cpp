#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "src/Main/Model/Structures/String/Buffer/StringBuffer.hpp"

using SB = StringBuffer::StringBuffer;

// ---------------------------------------------------------------------------
// Mini framework de testes
// ---------------------------------------------------------------------------

namespace {

int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::cerr << "  FALHOU (" << __FILE__ << ":" << __LINE__ << "): "  \
                      << #cond << "\n";                                        \
        }                                                                      \
    } while (0)

#define CHECK_THROWS(expr, ExceptionType)                                      \
    do {                                                                       \
        bool thrown = false;                                                   \
        try {                                                                  \
            expr;                                                              \
        } catch (const ExceptionType&) {                                       \
            thrown = true;                                                     \
        } catch (...) {                                                        \
        }                                                                      \
        if (!thrown) {                                                         \
            ++g_failures;                                                      \
            std::cerr << "  FALHOU (" << __FILE__ << ":" << __LINE__ << "): "  \
                      << #expr << " deveria lancar " << #ExceptionType << "\n";\
        }                                                                      \
    } while (0)

using TestFn = void (*)();

void run(const char* name, TestFn fn) {
    const int before = g_failures;
    try {
        fn();
    } catch (const std::exception& e) {
        ++g_failures;
        std::cerr << "  EXCECAO INESPERADA: " << e.what() << "\n";
    }
    std::cout << (g_failures == before ? "[OK]    " : "[FALHA] ") << name << "\n";
}

// ---------------------------------------------------------------------------
// Construção, cópia, movimentação
// ---------------------------------------------------------------------------

void testDefaultConstructor() {
    SB sb;
    CHECK(sb.empty());
    CHECK(sb.size() == 0);
    CHECK(sb.capacity() == SB::kDefaultCapacity);
    CHECK(std::string(sb.c_str()).empty());
}

void testCapacityConstructor() {
    SB sb(64);
    CHECK(sb.empty());
    CHECK(sb.capacity() == 64);
}

void testContentConstructor() {
    SB sb{BufferView("hello")};
    CHECK(sb.str() == "hello");
    CHECK(sb.size() == 5);
    CHECK(sb.capacity() >= sb.size());
}

void testCopy() {
    SB original{BufferView("abc")};
    original.reserve(50);

    SB copy(original);
    CHECK(copy.str() == "abc");
    CHECK(copy.capacity() == original.capacity());

    copy.pushBack('d');  // a cópia deve ser independente
    CHECK(original.str() == "abc");
    CHECK(copy.str() == "abcd");

    SB assigned;
    assigned = original;
    CHECK(assigned.str() == "abc");
}

void testMove() {
    SB source{BufferView("moved")};
    SB target(std::move(source));
    CHECK(target.str() == "moved");

    // Objeto movido: vazio, válido e reutilizável.
    CHECK(source.empty());
    CHECK(source.capacity() == 0);
    CHECK(std::string(source.c_str()).empty());
    source.append("again");
    CHECK(source.str() == "again");

    SB other;
    other = std::move(target);
    CHECK(other.str() == "moved");
    CHECK(target.empty());
}

void testSwap() {
    SB a{BufferView("one")};
    SB b{BufferView("two2")};
    a.swap(b);
    CHECK(a.str() == "two2");
    CHECK(b.str() == "one");
}

// ---------------------------------------------------------------------------
// Inserção
// ---------------------------------------------------------------------------

void testInsert() {
    SB sb{BufferView("ac")};
    sb.insert(1, 'b');
    CHECK(sb.str() == "abc");

    sb.insert(0, 2, 'x');
    CHECK(sb.str() == "xxabc");

    sb.insert(sb.size(), BufferView("!!"));
    CHECK(sb.str() == "xxabc!!");

    sb.insert(2, BufferView(""));  // inserir vazio não altera nada
    CHECK(sb.str() == "xxabc!!");
}

void testInsertOwnContent() {
    // Inserir uma view do próprio buffer deve funcionar (aliasing).
    SB sb{BufferView("abc")};
    sb.insert(1, sb.view());
    CHECK(sb.str() == "aabcbc");

    SB grow(3);  // força realocação durante o append do próprio conteúdo
    grow.append("abc");
    grow.append(grow.view());
    CHECK(grow.str() == "abcabc");
}

void testAppendAndPushBack() {
    SB sb;
    sb.append("foo").append(2, '-').append("bar");
    sb.pushBack('!');
    CHECK(sb.str() == "foo--bar!");
}

void testGrowthBeyondCapacity() {
    SB sb;
    const std::string big(200, 'z');
    sb.append(BufferView(big));
    CHECK(sb.size() == 200);
    CHECK(sb.capacity() >= 200);
    CHECK(sb.str() == big);
}

// ---------------------------------------------------------------------------
// Remoção e substituição
// ---------------------------------------------------------------------------

void testErase() {
    SB sb{BufferView("hello world")};
    sb.erase(5, 6);
    CHECK(sb.str() == "hello");

    sb.erase(1, 2);
    CHECK(sb.str() == "hlo");

    sb.erase(1);  // count padrão (npos): remove até o fim
    CHECK(sb.str() == "h");

    sb.erase(0, 100);  // count acima do disponível é limitado
    CHECK(sb.empty());
}

void testClearKeepsCapacity() {
    SB sb;
    sb.append("some content");
    sb.reserve(100);
    sb.clear();
    CHECK(sb.empty());
    CHECK(sb.capacity() == 100);
    CHECK(std::string(sb.c_str()).empty());
}

void testReplace() {
    SB sb{BufferView("hello world")};
    sb.replace(0, 5, BufferView("HI"));
    CHECK(sb.str() == "HI world");

    sb.replace(3, 100, BufferView("there"));  // count limitado ao existente
    CHECK(sb.str() == "HI there");

    sb.replace(0, 2, BufferView("Greetings,"));  // texto maior que o removido
    CHECK(sb.str() == "Greetings, there");

    sb.replace(0, 0, sb.view().substr(0, 3));  // origem dentro do próprio buffer
    CHECK(sb.str() == "GreGreetings, there");
}

void testReplaceAll() {
    SB sb{BufferView("a-b-c")};
    CHECK(sb.replaceAll(BufferView("-"), BufferView("+")) == 2);
    CHECK(sb.str() == "a+b+c");

    SB grow{BufferView("aaa")};
    CHECK(grow.replaceAll(BufferView("a"), BufferView("bb")) == 3);
    CHECK(grow.str() == "bbbbbb");

    SB shrink{BufferView("xxxx")};  // ocorrências não sobrepostas
    CHECK(shrink.replaceAll(BufferView("xx"), BufferView("y")) == 2);
    CHECK(shrink.str() == "yy");

    SB none{BufferView("abc")};
    CHECK(none.replaceAll(BufferView("zzz"), BufferView("q")) == 0);
    CHECK(none.str() == "abc");
}

// ---------------------------------------------------------------------------
// Capacidade e tamanho
// ---------------------------------------------------------------------------

void testReserve() {
    SB sb{BufferView("abc")};
    sb.reserve(128);
    CHECK(sb.capacity() == 128);
    CHECK(sb.str() == "abc");

    sb.reserve(10);  // nunca diminui
    CHECK(sb.capacity() == 128);
}

void testResize() {
    SB sb{BufferView("abc")};
    sb.resize(6, 'x');
    CHECK(sb.str() == "abcxxx");

    const std::size_t capacityBefore = sb.capacity();
    sb.resize(2);
    CHECK(sb.str() == "ab");
    CHECK(sb.capacity() == capacityBefore);  // truncar não libera memória

    sb.resize(4);  // fill padrão é '\0'
    CHECK(sb.size() == 4);
    CHECK(sb[2] == '\0' && sb[3] == '\0');
}

void testShrinkToFit() {
    SB sb;
    sb.append("abc");
    sb.shrinkToFit();
    CHECK(sb.capacity() == 3);
    CHECK(sb.str() == "abc");
}

// ---------------------------------------------------------------------------
// Acesso
// ---------------------------------------------------------------------------

void testAccess() {
    SB sb{BufferView("abc")};
    CHECK(sb[0] == 'a');
    CHECK(sb.at(2) == 'c');

    sb[1] = 'X';
    sb.at(2) = 'Y';
    CHECK(sb.str() == "aXY");

    CHECK(std::string(sb.c_str()) == "aXY");
    CHECK(sb.view() == BufferView("aXY"));

    const SB& constRef = sb;
    CHECK(constRef[0] == 'a');
    CHECK(constRef.at(1) == 'X');
}

// ---------------------------------------------------------------------------
// Exceções (e garantia de que o conteúdo não é alterado)
// ---------------------------------------------------------------------------

void testExceptions() {
    SB sb{BufferView("abc")};

    CHECK_THROWS(sb.at(3), std::out_of_range);
    CHECK_THROWS(sb.insert(4, 'x'), std::out_of_range);
    CHECK_THROWS(sb.erase(4, 1), std::out_of_range);
    CHECK_THROWS(sb.replace(4, 1, BufferView("x")), std::out_of_range);
    CHECK_THROWS(sb.replaceAll(BufferView(""), BufferView("x")), std::invalid_argument);

    const std::size_t huge = std::numeric_limits<std::size_t>::max();
    CHECK_THROWS(sb.reserve(huge), std::length_error);
    CHECK_THROWS(sb.resize(huge), std::length_error);
    CHECK_THROWS(SB(huge), std::length_error);

    CHECK(sb.str() == "abc");  // nada foi alterado
}

} // namespace

int main() {
    run("construtor padrao", testDefaultConstructor);
    run("construtor com capacidade", testCapacityConstructor);
    run("construtor com conteudo", testContentConstructor);
    run("copia", testCopy);
    run("movimentacao", testMove);
    run("swap", testSwap);

    run("insert", testInsert);
    run("insert do proprio conteudo", testInsertOwnContent);
    run("append e pushBack", testAppendAndPushBack);
    run("crescimento alem da capacidade", testGrowthBeyondCapacity);

    run("erase", testErase);
    run("clear preserva capacidade", testClearKeepsCapacity);
    run("replace", testReplace);
    run("replaceAll", testReplaceAll);

    run("reserve", testReserve);
    run("resize", testResize);
    run("shrinkToFit", testShrinkToFit);

    run("acesso", testAccess);
    run("excecoes", testExceptions);

    if (g_failures == 0) {
        std::cout << "\nTodos os testes passaram.\n";
        return 0;
    }
    std::cout << "\n" << g_failures << " verificacao(oes) falharam.\n";
    return 1;
}
