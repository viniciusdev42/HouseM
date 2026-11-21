#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/Main/Model/Entities/Project/Project.hpp"

using Proj = ::Project::Project;
using ::Project::ProjectStatus;
using ::Project::TaskId;
using ::Project::Clock;
using ::Project::TimePoint;
using ::Project::toString;
using std::chrono::hours;

namespace {

int g_total = 0;
int g_failed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_total;                                                               \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond "\n";     \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(ExceptionType, expr)                                        \
    do {                                                                         \
        ++g_total;                                                               \
        bool thrown = false;                                                     \
        try {                                                                    \
            expr;                                                                \
        } catch (const ExceptionType&) {                                         \
            thrown = true;                                                       \
        } catch (...) {                                                          \
        }                                                                        \
        if (!thrown) {                                                           \
            ++g_failed;                                                          \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): esperava "        \
                      << #ExceptionType << " em: " #expr "\n";                   \
        }                                                                        \
    } while (0)

// AJUSTE AQUI caso TaskId seja construído de outra forma.
TaskId makeTask(const std::string& value) { return TaskId(value); }

Proj makeProject(TimePoint createdAt = Clock::now()) {
    return Proj(1, "Meu Projeto", "Descricao", std::nullopt, createdAt);
}

void run(const char* name, void (*test)()) {
    std::cout << "[teste] " << name << "\n";
    test();
}

// --- toString ---

void testToString() {
    CHECK(toString(ProjectStatus::Active) == "Active");
    CHECK(toString(ProjectStatus::Completed) == "Completed");
    CHECK(toString(ProjectStatus::Archived) == "Archived");
}

// --- Construtor ---

void testConstructorValid() {
    const auto now = Clock::now();
    Proj p(7, "  Nome  ", "desc", now + hours(1), now);
    CHECK(p.id() == 7);
    CHECK(p.name() == "Nome");  // trim
    CHECK(p.description() == "desc");
    CHECK(p.isActive());
    CHECK(p.taskCount() == 0);
    CHECK(p.createdAt() == now);
    CHECK(p.updatedAt() == now);
    CHECK(p.deadline().has_value());
}

void testConstructorInvalid() {
    const auto now = Clock::now();
    CHECK_THROWS(std::invalid_argument, Proj(Proj::kInvalidId, "X", {}, std::nullopt, now));
    CHECK_THROWS(std::invalid_argument, Proj(1, "   ", {}, std::nullopt, now));
    CHECK_THROWS(std::invalid_argument,
                 Proj(1, std::string(Proj::kMaxNameLength + 1, 'a'), {}, std::nullopt, now));
    CHECK_THROWS(std::invalid_argument,
                 Proj(1, "X", std::string(Proj::kMaxDescriptionLength + 1, 'a'),
                         std::nullopt, now));
    CHECK_THROWS(std::invalid_argument, Proj(1, "X", {}, now - hours(1), now));
}

// --- restore ---

void testRestore() {
    const auto created = Clock::now();
    const auto updated = created + hours(2);
    auto p = Proj::restore(3, "Nome", "desc", ProjectStatus::Completed, std::nullopt,
                              created, updated, {makeTask("a"), makeTask("b")});
    CHECK(p.isCompleted());
    CHECK(p.updatedAt() == updated);
    CHECK(p.taskCount() == 2);
    CHECK(p.hasTask(makeTask("a")));

    CHECK_THROWS(std::invalid_argument,
                 Proj::restore(3, "Nome", "", ProjectStatus::Active, std::nullopt,
                                  created, created - hours(1), {}));
    CHECK_THROWS(std::invalid_argument,
                 Proj::restore(3, "Nome", "", ProjectStatus::Active, std::nullopt,
                                  created, created, {makeTask("a"), makeTask("a")}));
}

// --- Consultas ---

void testIsOverdue() {
    const auto now = Clock::now();
    Proj p(1, "X", {}, now + hours(1), now);
    CHECK(!p.isOverdue(now));
    CHECK(p.isOverdue(now + hours(2)));

    p.complete(now);
    CHECK(!p.isOverdue(now + hours(2)));  // só Active pode estar atrasado

    Proj semPrazo = makeProject(now);
    CHECK(!semPrazo.isOverdue(now + hours(100)));
}

// --- Metadados ---

void testMetadata() {
    const auto t0 = Clock::now();
    Proj p = makeProject(t0);

    p.rename("  Novo  ", t0 + hours(1));
    CHECK(p.name() == "Novo");
    CHECK(p.updatedAt() == t0 + hours(1));
    CHECK_THROWS(std::invalid_argument, p.rename("   "));

    p.setDescription("outra", t0 + hours(2));
    CHECK(p.description() == "outra");
    CHECK_THROWS(std::invalid_argument,
                 p.setDescription(std::string(Proj::kMaxDescriptionLength + 1, 'a')));

    p.setDeadline(t0 + hours(5), t0 + hours(3));
    CHECK(p.deadline() == t0 + hours(5));
    CHECK_THROWS(std::invalid_argument, p.setDeadline(t0 - hours(1)));

    p.clearDeadline(t0 + hours(4));
    CHECK(!p.deadline().has_value());
}

void testTouchNeverGoesBackwards() {
    const auto t0 = Clock::now();
    Proj p = makeProject(t0);
    p.rename("Novo", t0 + hours(5));
    p.rename("Outro", t0 + hours(1));  // 'now' mais antigo
    CHECK(p.updatedAt() == t0 + hours(5));
}

// --- Tarefas ---

void testTasks() {
    const auto t0 = Clock::now();
    Proj p = makeProject(t0);

    CHECK(p.addTask(makeTask("t1"), t0 + hours(1)));
    CHECK(p.hasTask(makeTask("t1")));
    CHECK(p.taskCount() == 1);
    CHECK(p.updatedAt() == t0 + hours(1));
    CHECK(!p.addTask(makeTask("t1"), t0 + hours(2)));  // duplicada
    CHECK(p.taskCount() == 1);

    CHECK(!p.removeTask(makeTask("inexistente")));
    CHECK(p.removeTask(makeTask("t1"), t0 + hours(3)));
    CHECK(!p.hasTask(makeTask("t1")));
    CHECK(p.taskCount() == 0);
}

void testTasksRestrictedByStatus() {
    Proj p = makeProject();
    p.addTask(makeTask("t1"));

    p.complete();
    CHECK_THROWS(std::logic_error, p.addTask(makeTask("t2")));  // Completed não aceita novas
    CHECK(p.removeTask(makeTask("t1")));                         // mas aceita remoção

    p.archive();
    CHECK_THROWS(std::logic_error, p.addTask(makeTask("t3")));
    CHECK_THROWS(std::logic_error, p.removeTask(makeTask("t3")));
}

// --- Transições ---

void testTransitions() {
    Proj p = makeProject();

    CHECK_THROWS(std::logic_error, p.reopen());     // Active -> reopen inválido
    CHECK_THROWS(std::logic_error, p.unarchive());  // Active -> unarchive inválido

    p.complete();
    CHECK(p.isCompleted());
    CHECK_THROWS(std::logic_error, p.complete());

    p.reopen();
    CHECK(p.isActive());

    p.archive();
    CHECK(p.isArchived());
    CHECK_THROWS(std::logic_error, p.archive());
    CHECK_THROWS(std::logic_error, p.complete());
    CHECK_THROWS(std::logic_error, p.reopen());

    p.unarchive();
    CHECK(p.isActive());
}

void testArchivedBlocksMetadataEdits() {
    Proj p = makeProject();
    p.archive();
    CHECK_THROWS(std::logic_error, p.rename("Novo"));
    CHECK_THROWS(std::logic_error, p.setDescription("x"));
    CHECK_THROWS(std::logic_error, p.setDeadline(Clock::now() + hours(1)));
    CHECK_THROWS(std::logic_error, p.clearDeadline());
}

// --- Igualdade ---

void testEquality() {
    Proj a(1, "A");
    Proj b(1, "B");
    Proj c(2, "A");
    CHECK(a == b);  // igualdade por id
    CHECK(a != c);
}

}  // namespace

int main() {
    run("toString", testToString);
    run("construtor valido", testConstructorValid);
    run("construtor invalido", testConstructorInvalid);
    run("restore", testRestore);
    run("isOverdue", testIsOverdue);
    run("metadados", testMetadata);
    run("touch nao retrocede", testTouchNeverGoesBackwards);
    run("tarefas", testTasks);
    run("tarefas x status", testTasksRestrictedByStatus);
    run("transicoes", testTransitions);
    run("arquivado bloqueia edicoes", testArchivedBlocksMetadataEdits);
    run("igualdade", testEquality);

    std::cout << "\n" << (g_total - g_failed) << "/" << g_total << " verificacoes passaram.\n";
    return g_failed == 0 ? 0 : 1;
}
