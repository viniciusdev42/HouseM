#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "src/Model/Entities/Dependency/Dependency.hpp"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "    [FALHA] " << __FILE__ << ":" << __LINE__ << "  "   \
                      << #cond << "\n";                                          \
        }                                                                        \
    } while (false)

#define CHECK_THROWS_INVALID_ARGUMENT(expr)                                      \
    do {                                                                         \
        ++g_checks;                                                              \
        bool threw = false;                                                      \
        try {                                                                    \
            expr;                                                                \
        } catch (const std::invalid_argument&) {                                 \
            threw = true;                                                        \
        } catch (...) {                                                          \
        }                                                                        \
        if (!threw) {                                                            \
            ++g_failures;                                                        \
            std::cerr << "    [FALHA] " << __FILE__ << ":" << __LINE__           \
                      << "  esperava std::invalid_argument em: " << #expr << "\n"; \
        }                                                                        \
    } while (false)

using Dependency::Dependency;

// --- Auxiliares ------------------------------------------------------------

Task::TaskId id(const char* value) { return Task::TaskId(value); }

// ID considerado inválido pela Dependency (ID vazio). Se a regra mudar,
// basta ajustar este ponto.
Task::TaskId invalidId() { return Task::TaskId(""); }

Task::Task makeTask(const char* taskId, const char* title) {
    return Task::Task::create(id(taskId), title, "descricao", Task::Priority::Medium,
                              std::nullopt, Task::Task::TimePoint{});
}

// --- Construção ------------------------------------------------------------

void testConstrutorValidoGuardaIds() {
    const Dependency dep(id("t1"), id("t2"));
    CHECK(dep.prerequisiteId() == id("t1"));
    CHECK(dep.dependentId() == id("t2"));
}

void testConstrutorLancaParaIdInvalido() {
    CHECK_THROWS_INVALID_ARGUMENT(Dependency(invalidId(), id("t2")));
    CHECK_THROWS_INVALID_ARGUMENT(Dependency(id("t1"), invalidId()));
}

void testConstrutorLancaParaAutoDependencia() {
    CHECK_THROWS_INVALID_ARGUMENT(Dependency(id("t3"), id("t3")));
}

// --- isValidPair -----------------------------------------------------------

void testIsValidPair() {
    CHECK(Dependency::isValidPair(id("t1"), id("t2")));
    CHECK(Dependency::isValidPair(id("t2"), id("t1")));
    CHECK(!Dependency::isValidPair(invalidId(), id("t2")));
    CHECK(!Dependency::isValidPair(id("t1"), invalidId()));
    CHECK(!Dependency::isValidPair(id("t4"), id("t4")));
}

// --- Consultas -------------------------------------------------------------

void testInvolves() {
    const Dependency dep(id("t1"), id("t2"));
    CHECK(dep.involves(id("t1")));
    CHECK(dep.involves(id("t2")));
    CHECK(!dep.involves(id("t3")));
}

void testToString() {
    CHECK(Dependency(id("t1"), id("t2")).toString() == "t1 -> t2");
    CHECK(Dependency(id("t2"), id("t1")).toString() == "t2 -> t1");
}

// --- Identidade ------------------------------------------------------------

void testIgualdade() {
    CHECK(Dependency(id("t1"), id("t2")) == Dependency(id("t1"), id("t2")));
    CHECK(!(Dependency(id("t1"), id("t2")) != Dependency(id("t1"), id("t2"))));
    CHECK(Dependency(id("t1"), id("t2")) != Dependency(id("t2"), id("t1")));  // ordem importa
    CHECK(Dependency(id("t1"), id("t2")) != Dependency(id("t1"), id("t3")));
}

void testOrdenacaoLexicografica() {
    CHECK(Dependency(id("t1"), id("t2")) < Dependency(id("t2"), id("t1")));  // pré-requisito primeiro
    CHECK(Dependency(id("t1"), id("t2")) < Dependency(id("t1"), id("t3")));  // empate: dependente
    CHECK(!(Dependency(id("t1"), id("t3")) < Dependency(id("t1"), id("t2"))));
    CHECK(!(Dependency(id("t1"), id("t2")) < Dependency(id("t1"), id("t2"))));

    std::set<Dependency> ordenadas{Dependency(id("t2"), id("t1")),
                                   Dependency(id("t1"), id("t3")),
                                   Dependency(id("t1"), id("t2"))};
    CHECK(ordenadas.size() == 3);
    CHECK(*ordenadas.begin() == Dependency(id("t1"), id("t2")));
}

// --- Hash ------------------------------------------------------------------

void testHash() {
    const Dependency dep(id("t1"), id("t2"));
    // Mesmo par -> mesmo hash.
    CHECK(dep.hashValue() == Dependency(id("t1"), id("t2")).hashValue());
    // Pares simétricos não devem colidir (a ordem importa).
    CHECK(dep.hashValue() != Dependency(id("t2"), id("t1")).hashValue());
    // Especialização de std::hash delega para hashValue().
    CHECK(std::hash<Dependency>{}(dep) == dep.hashValue());
}

void testUsoEmContainerComHash() {
    std::unordered_set<Dependency> conjunto;
    conjunto.insert(Dependency(id("t1"), id("t2")));
    conjunto.insert(Dependency(id("t1"), id("t2")));  // duplicata
    conjunto.insert(Dependency(id("t2"), id("t1")));
    CHECK(conjunto.size() == 2);
    CHECK(conjunto.count(Dependency(id("t1"), id("t2"))) == 1);
}

// --- Integração com Task ---------------------------------------------------

void testCriaDependenciaAPartirDeTasks() {
    const Task::Task prerequisito = makeTask("t1", "Escrever codigo");
    const Task::Task dependente = makeTask("t2", "Revisar codigo");

    const Dependency dep(prerequisito.id(), dependente.id());
    CHECK(dep.prerequisiteId() == prerequisito.id());
    CHECK(dep.dependentId() == dependente.id());
    CHECK(dep.involves(prerequisito.id()));
    CHECK(dep.involves(dependente.id()));
}

void testDependenciaNaoAlteraEstadoDasTasks() {
    const Task::Task prerequisito = makeTask("t1", "Escrever codigo");
    const Task::Task dependente = makeTask("t2", "Revisar codigo");

    const Dependency dep(prerequisito.id(), dependente.id());
    (void)dep;
    CHECK(prerequisito.isPending());
    CHECK(dependente.isPending());
}

void testTaskNaoPodeDependerDeSiMesma() {
    const Task::Task tarefa = makeTask("t1", "Escrever codigo");
    CHECK(!Dependency::isValidPair(tarefa.id(), tarefa.id()));
    CHECK_THROWS_INVALID_ARGUMENT(Dependency(tarefa.id(), tarefa.id()));
}

// --- Execução --------------------------------------------------------------

struct TestCase {
    const char* name;
    void (*fn)();
};

}  // namespace

int main() {
    const TestCase tests[] = {
        {"Construtor valido guarda os IDs", testConstrutorValidoGuardaIds},
        {"Construtor lanca para ID invalido", testConstrutorLancaParaIdInvalido},
        {"Construtor lanca para auto-dependencia", testConstrutorLancaParaAutoDependencia},
        {"isValidPair", testIsValidPair},
        {"involves", testInvolves},
        {"toString", testToString},
        {"Igualdade e desigualdade", testIgualdade},
        {"Ordenacao lexicografica", testOrdenacaoLexicografica},
        {"hashValue e std::hash", testHash},
        {"Uso em unordered_set", testUsoEmContainerComHash},
        {"Integracao: dependencia a partir de Tasks", testCriaDependenciaAPartirDeTasks},
        {"Integracao: nao altera o estado das Tasks", testDependenciaNaoAlteraEstadoDasTasks},
        {"Integracao: Task nao depende de si mesma", testTaskNaoPodeDependerDeSiMesma},
    };

    int failedTests = 0;
    for (const auto& test : tests) {
        const int failuresBefore = g_failures;
        test.fn();
        const bool ok = (g_failures == failuresBefore);
        if (!ok) ++failedTests;
        std::cout << (ok ? "[ OK ] " : "[FAIL] ") << test.name << "\n";
    }

    const std::size_t total = sizeof(tests) / sizeof(tests[0]);
    std::cout << "\n" << (total - failedTests) << "/" << total << " testes passaram ("
              << g_checks << " verificacoes).\n";
    return failedTests == 0 ? 0 : 1;
}
