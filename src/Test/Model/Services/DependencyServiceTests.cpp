#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/Main/Model/Services/Dependency/DependencyService.hpp"

using Dependency::DependencyService;
using TaskId = DependencyService::TaskId;
using TaskEntity = Task::Task;
using Repository = ITaskRepository::InMemoryTaskRepository;

// --- Mini framework --------------------------------------------------------

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond "\n";     \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr, ExceptionType)                                        \
    do {                                                                         \
        ++g_checks;                                                              \
        bool thrown = false;                                                     \
        try {                                                                    \
            (void)(expr);                                                        \
        } catch (const ExceptionType&) {                                         \
            thrown = true;                                                       \
        } catch (...) {                                                          \
        }                                                                        \
        if (!thrown) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): esperava "        \
                      << #ExceptionType << " em " #expr "\n";                    \
        }                                                                        \
    } while (0)

#define RUN(test)                                                                \
    do {                                                                         \
        std::cout << "[ RUN ] " #test "\n";                                      \
        test();                                                                  \
    } while (0)

// --- Auxiliares ------------------------------------------------------------

static TaskId id(const std::string& value) { return TaskId(value); }

// Repositório com as tarefas "A", "B", "C" e "D" já cadastradas.
struct Fixture {
    Repository repo;
    Fixture() {
        for (const char* name : {"A", "B", "C", "D"}) {
            repo.add(TaskEntity::create(id(name), "titulo", "descricao",
                                        Task::Priority::Medium));
        }
    }
};

static std::vector<std::string> idsOf(const std::vector<TaskEntity>& tasks) {
    std::vector<std::string> result;
    for (const auto& task : tasks) {
        result.push_back(task.id().value());
    }
    return result;
}

static bool contains(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

// --- Escrita ---------------------------------------------------------------

static void test_addDependency_registersDependency() {
    Fixture f;
    DependencyService service(f.repo);

    CHECK(service.addDependency(id("A"), id("B")));
    CHECK(service.hasDependency(id("A"), id("B")));
    CHECK(!service.hasDependency(id("B"), id("A")));  // direção importa
}

static void test_addDependency_duplicateReturnsFalse() {
    Fixture f;
    DependencyService service(f.repo);

    CHECK(service.addDependency(id("A"), id("B")));
    CHECK(!service.addDependency(id("A"), id("B")));
}

static void test_addDependency_invalidPairThrows() {
    Fixture f;
    DependencyService service(f.repo);

    CHECK_THROWS(service.addDependency(id("A"), id("A")), std::invalid_argument);
}

static void test_addDependency_unknownTaskThrows() {
    Fixture f;
    DependencyService service(f.repo);

    CHECK_THROWS(service.addDependency(id("A"), id("X")), ITaskRepository::TaskNotFoundError);
    CHECK_THROWS(service.addDependency(id("X"), id("A")), ITaskRepository::TaskNotFoundError);
}

static void test_addDependency_cycleThrows() {
    Fixture f;
    DependencyService service(f.repo);

    service.addDependency(id("A"), id("B"));
    service.addDependency(id("B"), id("C"));

    CHECK_THROWS(service.addDependency(id("C"), id("A")), std::logic_error);
    CHECK(!service.hasDependency(id("C"), id("A")));  // estado não foi alterado
}

static void test_removeDependency() {
    Fixture f;
    DependencyService service(f.repo);

    service.addDependency(id("A"), id("B"));

    CHECK(service.removeDependency(id("A"), id("B")));
    CHECK(!service.hasDependency(id("A"), id("B")));
    CHECK(!service.removeDependency(id("A"), id("B")));  // já removida
    CHECK(!service.removeDependency(id("C"), id("D")));  // nunca existiu
}

static void test_removeTaskDependencies() {
    Fixture f;
    DependencyService service(f.repo);

    service.addDependency(id("A"), id("B"));
    service.addDependency(id("B"), id("C"));
    service.addDependency(id("B"), id("D"));

    CHECK(service.removeTaskDependencies(id("B")) == 3);
    CHECK(!service.hasDependency(id("A"), id("B")));
    CHECK(!service.hasDependency(id("B"), id("C")));
    CHECK(!service.hasDependency(id("B"), id("D")));
    CHECK(service.removeTaskDependencies(id("B")) == 0);  // nada restou
}

// --- Consultas -------------------------------------------------------------

static void test_getDependencies_and_getDependents() {
    Fixture f;
    DependencyService service(f.repo);

    service.addDependency(id("A"), id("C"));
    service.addDependency(id("B"), id("C"));
    service.addDependency(id("C"), id("D"));

    CHECK(service.getDependencies(id("C")).size() == 2);  // A e B
    CHECK(service.getDependents(id("C")).size() == 1);    // D
    CHECK(service.getDependencies(id("A")).empty());
    CHECK(service.getDependents(id("D")).empty());
    CHECK(service.getDependencies(id("X")).empty());      // desconhecida
}

static void test_hasPath_transitive() {
    Fixture f;
    DependencyService service(f.repo);

    service.addDependency(id("A"), id("B"));
    service.addDependency(id("B"), id("C"));

    CHECK(service.hasPath(id("A"), id("C")));
    CHECK(!service.hasPath(id("C"), id("A")));
    CHECK(!service.hasPath(id("A"), id("D")));  // D não é rastreada
}

static void test_detectCycle() {
    Fixture f;
    DependencyService service(f.repo);

    service.addDependency(id("A"), id("B"));
    service.addDependency(id("B"), id("C"));

    CHECK(!service.detectCycle());                    // estado atual é acíclico
    CHECK(service.detectCycle(id("C"), id("A")));     // fecharia ciclo
    CHECK(service.detectCycle(id("A"), id("A")));     // laço
    CHECK(!service.detectCycle(id("A"), id("C")));    // atalho válido
    CHECK(!service.hasDependency(id("C"), id("A")));  // consulta não altera estado
}

static void test_blockedAndAvailableTasks() {
    Fixture f;
    DependencyService service(f.repo);

    service.addDependency(id("A"), id("B"));
    service.addDependency(id("B"), id("C"));

    // Nada concluído: só A está livre.
    auto available = idsOf(service.getAvailableTasks({}));
    auto blocked = idsOf(service.getBlockedTasks({}));
    CHECK(available.size() == 1 && contains(available, "A"));
    CHECK(blocked.size() == 2 && contains(blocked, "B") && contains(blocked, "C"));

    // A concluída: B liberada, C continua bloqueada.
    const std::set<TaskId> completed{id("A")};
    available = idsOf(service.getAvailableTasks(completed));
    blocked = idsOf(service.getBlockedTasks(completed));
    CHECK(available.size() == 1 && contains(available, "B"));
    CHECK(blocked.size() == 1 && contains(blocked, "C"));

    // Alias com a grafia do enunciado.
    CHECK(idsOf(service.getAvaliableTasks(completed)) == available);
}

static void test_blockedTasks_missingTaskThrows() {
    Fixture f;
    DependencyService service(f.repo);

    service.addDependency(id("A"), id("B"));
    f.repo.remove(id("B"));  // o serviço não é avisado

    CHECK_THROWS(service.getBlockedTasks({}), ITaskRepository::TaskNotFoundError);
}

static void test_getTopologicalOrder() {
    Fixture f;
    DependencyService service(f.repo);

    service.addDependency(id("A"), id("B"));
    service.addDependency(id("A"), id("C"));
    service.addDependency(id("B"), id("D"));
    service.addDependency(id("C"), id("D"));

    const auto order = idsOf(service.getTopologicalOrder());
    CHECK(order.size() == 4);

    auto pos = [&](const std::string& name) {
        return std::find(order.begin(), order.end(), name) - order.begin();
    };
    CHECK(pos("A") < pos("B"));
    CHECK(pos("A") < pos("C"));
    CHECK(pos("B") < pos("D"));
    CHECK(pos("C") < pos("D"));
}

// --- main ------------------------------------------------------------------

int main() {
    RUN(test_addDependency_registersDependency);
    RUN(test_addDependency_duplicateReturnsFalse);
    RUN(test_addDependency_invalidPairThrows);
    RUN(test_addDependency_unknownTaskThrows);
    RUN(test_addDependency_cycleThrows);
    RUN(test_removeDependency);
    RUN(test_removeTaskDependencies);
    RUN(test_getDependencies_and_getDependents);
    RUN(test_hasPath_transitive);
    RUN(test_detectCycle);
    RUN(test_blockedAndAvailableTasks);
    RUN(test_blockedTasks_missingTaskThrows);
    RUN(test_getTopologicalOrder);

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " verificacoes passaram.\n";
    return g_failures == 0 ? 0 : 1;
}
