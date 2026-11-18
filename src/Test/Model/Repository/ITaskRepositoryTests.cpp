// Testes de InMemoryTaskRepository (sem frameworks externos), incluindo
// integração com a entidade Task.
//
// Compilar (exemplo):
//   g++ -std=c++17 -I<raiz_do_projeto> ITaskRepositoryTests.cpp -o repo_tests -pthread
//
// Observação: Task.cpp (implementação de Task::create, start, complete, etc.)
// precisa ser compilado/linkado junto, caso não seja header-only.

#include <chrono>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/Model/Repositories/ITaskRepository.hpp"  



// ---------------------------------------------------------------------------
// Aliases (o namespace e a classe se chamam Task: Task::Task)
// ---------------------------------------------------------------------------
using TaskEntity = Task::Task;
using TaskId = Task::TaskId;
using Status = Task::Status;
using Priority = Task::Priority;
using Clock = TaskEntity::Clock;
using TimePoint = TaskEntity::TimePoint;

using ITaskRepository::DuplicateTaskError;
using ITaskRepository::InMemoryTaskRepository;
using ITaskRepository::TaskNotFoundError;

constexpr Status kOpen = Status::Pending;
constexpr Status kDone = Status::Completed;
constexpr Priority kLow = Priority::Low;
constexpr Priority kHigh = Priority::Critical;

// ---------------------------------------------------------------------------
// Mini-harness
// ---------------------------------------------------------------------------
struct TestFailure {
    std::string message;
};

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::ostringstream oss_;                                                  \
            oss_ << __FILE__ << ":" << __LINE__ << ": CHECK falhou: " #cond;         \
            throw TestFailure{oss_.str()};                                            \
        }                                                                             \
    } while (false)

#define CHECK_THROWS_AS(expr, ExceptionType)                                          \
    do {                                                                              \
        bool thrown_ = false;                                                         \
        try {                                                                         \
            (void)(expr);                                                             \
        } catch (const ExceptionType&) {                                              \
            thrown_ = true;                                                           \
        } catch (...) {                                                               \
        }                                                                             \
        if (!thrown_) {                                                               \
            std::ostringstream oss_;                                                  \
            oss_ << __FILE__ << ":" << __LINE__ << ": esperava " #ExceptionType       \
                 << " em: " #expr;                                                    \
            throw TestFailure{oss_.str()};                                            \
        }                                                                             \
    } while (false)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
using Ids = std::vector<std::string>;

// Instante base fixo e deslocamento em horas, para testes determinísticos.
const TimePoint kBase = Clock::now();
TimePoint at(int hours) { return kBase + std::chrono::hours(hours); }

// Cria uma Task via Task::create e a leva ao status desejado pelas transições
// reais da entidade (Pending -> InProgress -> Completed).
// Todos os deadlines usados nos testes são posteriores a createdAt.
TaskEntity makeTask(const std::string& id, TimePoint createdAt, Status status = kOpen,
                    Priority priority = kLow, std::optional<TimePoint> deadline = std::nullopt) {
    TaskEntity task = TaskEntity::create(TaskId(id), "title " + id, "description " + id,
                                         priority, deadline, createdAt);
    if (status != Status::Pending) {
        task.start(createdAt);
    }
    if (status == Status::Completed) {
        task.complete(createdAt);
    }
    return task;
}

// Converte um resultado em lista de ids, para comparar conteúdo e ordem.
Ids ids(const std::vector<TaskEntity>& tasks) {
    Ids out;
    for (const TaskEntity& task : tasks) {
        out.push_back(task.id().value());
    }
    return out;
}

// ---------------------------------------------------------------------------
// Create / Update / Delete
// ---------------------------------------------------------------------------
void addAndFind() {
    InMemoryTaskRepository repo;
    CHECK(repo.count() == 0);

    repo.add(makeTask("a", at(0)));

    CHECK(repo.count() == 1);
    CHECK(repo.existsById(TaskId("a")));
    const auto found = repo.findById(TaskId("a"));
    CHECK(found.has_value());
    CHECK(found->id().value() == "a");
    CHECK(!repo.findById(TaskId("zzz")).has_value());
    CHECK(!repo.existsById(TaskId("zzz")));
}

void addDuplicateThrows() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(0)));

    CHECK_THROWS_AS(repo.add(makeTask("a", at(1))), DuplicateTaskError);
    CHECK(repo.count() == 1);
}

void updateReplacesAndReindexes() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(0), kOpen, kLow, at(5)));
    CHECK(repo.countByStatus(kOpen) == 1);

    // Mesmo id, outros campos: status, prioridade e deadline mudam.
    repo.update(makeTask("a", at(0), kDone, kHigh, at(20)));

    CHECK(repo.count() == 1);
    CHECK(repo.findById(TaskId("a"))->status() == kDone);
    // Índices antigos não podem manter a tarefa.
    CHECK(repo.countByStatus(kOpen) == 0);
    CHECK(repo.findByStatus(kOpen).empty());
    CHECK(repo.findByPriority(kLow).empty());
    CHECK(repo.findByDeadlineBetween(at(0), at(10)).empty());
    // Índices novos devem refleti-la.
    CHECK(ids(repo.findByStatus(kDone)) == Ids{"a"});
    CHECK(ids(repo.findByPriority(kHigh)) == Ids{"a"});
    CHECK(ids(repo.findByDeadlineBetween(at(10), at(30))) == Ids{"a"});
}

void updateMissingThrows() {
    InMemoryTaskRepository repo;
    CHECK_THROWS_AS(repo.update(makeTask("a", at(0))), TaskNotFoundError);
}

void removeTask() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(0), kOpen, kLow, at(5)));

    CHECK(repo.remove(TaskId("a")));
    CHECK(!repo.remove(TaskId("a")));  // já removida
    CHECK(repo.count() == 0);
    CHECK(!repo.existsById(TaskId("a")));
    // Também some de todos os índices.
    CHECK(repo.findAll().empty());
    CHECK(repo.findByStatus(kOpen).empty());
    CHECK(repo.findByPriority(kLow).empty());
    CHECK(repo.findByDeadlineBetween(at(0), at(10)).empty());
}

// ---------------------------------------------------------------------------
// Read / consultas
// ---------------------------------------------------------------------------
void findAllOrderedByCreatedAt() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("c", at(3)));
    repo.add(makeTask("a", at(1)));
    repo.add(makeTask("b", at(2)));

    CHECK(ids(repo.findAll()) == (Ids{"a", "b", "c"}));
}

void findByStatusAndCount() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(2), kOpen));
    repo.add(makeTask("b", at(1), kOpen));
    repo.add(makeTask("c", at(3), kDone));

    CHECK(ids(repo.findByStatus(kOpen)) == (Ids{"b", "a"}));  // por createdAt
    CHECK(ids(repo.findByStatus(kDone)) == Ids{"c"});
    CHECK(repo.countByStatus(kOpen) == 2);
    CHECK(repo.countByStatus(kDone) == 1);
    CHECK(repo.count() == 3);
}

void findByPriority() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(2), kOpen, kHigh));
    repo.add(makeTask("b", at(1), kOpen, kHigh));
    repo.add(makeTask("c", at(3), kOpen, kLow));

    CHECK(ids(repo.findByPriority(kHigh)) == (Ids{"b", "a"}));
    CHECK(ids(repo.findByPriority(kLow)) == Ids{"c"});
}

void findByDeadlineBetween() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(0), kOpen, kLow, at(10)));
    repo.add(makeTask("b", at(0), kOpen, kLow, at(5)));
    repo.add(makeTask("c", at(0), kOpen, kLow, at(20)));
    repo.add(makeTask("d", at(0)));  // sem deadline: nunca aparece

    // Intervalo semiaberto [5, 20): inclui 5 e 10, exclui 20. Ordenado por deadline.
    CHECK(ids(repo.findByDeadlineBetween(at(5), at(20))) == (Ids{"b", "a"}));
    // Intervalo vazio é válido.
    CHECK(repo.findByDeadlineBetween(at(5), at(5)).empty());
    // Intervalo invertido é inválido.
    CHECK_THROWS_AS(repo.findByDeadlineBetween(at(20), at(5)), std::invalid_argument);
}

void findOverdue() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("late2", at(0), kOpen, kLow, at(8)));
    repo.add(makeTask("late1", at(0), kOpen, kLow, at(3)));
    repo.add(makeTask("done", at(0), kDone, kLow, at(2)));     // concluída: não vence
    repo.add(makeTask("future", at(0), kOpen, kLow, at(50)));  // ainda no prazo
    repo.add(makeTask("none", at(0)));                         // sem deadline

    // Ordenadas por deadline.
    CHECK(ids(repo.findOverdue(at(10))) == (Ids{"late1", "late2"}));
    CHECK(repo.findOverdue(at(1)).empty());
}

void findByCreatedBetween() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(1)));
    repo.add(makeTask("b", at(2)));
    repo.add(makeTask("c", at(3)));

    // [1, 3): inclui 1 e 2, exclui 3.
    CHECK(ids(repo.findByCreatedBetween(at(1), at(3))) == (Ids{"a", "b"}));
    CHECK(repo.findByCreatedBetween(at(2), at(2)).empty());
    CHECK_THROWS_AS(repo.findByCreatedBetween(at(3), at(1)), std::invalid_argument);
}

void sameTimestampDoesNotCollide() {
    // O id desempata chaves de índice com o mesmo instante.
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(1), kOpen, kLow, at(5)));
    repo.add(makeTask("b", at(1), kOpen, kLow, at(5)));

    CHECK(repo.findAll().size() == 2);
    CHECK(repo.findByStatus(kOpen).size() == 2);
    CHECK(repo.findByDeadlineBetween(at(5), at(6)).size() == 2);
}

// ---------------------------------------------------------------------------
// Integração com Task: o fluxo real "carregar -> mutar a entidade -> update"
// ---------------------------------------------------------------------------
void storedCopyIsIsolated() {
    InMemoryTaskRepository repo;
    TaskEntity original = makeTask("a", at(0));
    repo.add(original);

    // Mutar o objeto original depois do add não afeta o repositório.
    original.start(at(1));
    CHECK(repo.findById(TaskId("a"))->status() == Status::Pending);

    // Mutar uma cópia devolvida também não afeta o repositório.
    auto loaded = repo.findById(TaskId("a"));
    loaded->start(at(1));
    CHECK(repo.findById(TaskId("a"))->status() == Status::Pending);
    CHECK(repo.countByStatus(Status::Pending) == 1);
    CHECK(repo.countByStatus(Status::InProgress) == 0);
}

void lifecycleThroughRepository() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(0), kOpen, kLow, at(5)));

    // Pending -> InProgress
    auto task = repo.findById(TaskId("a"));
    task->start(at(1));
    repo.update(*task);
    CHECK(ids(repo.findByStatus(Status::InProgress)) == Ids{"a"});
    CHECK(repo.findByStatus(Status::Pending).empty());

    // InProgress -> Completed
    task = repo.findById(TaskId("a"));
    task->complete(at(2));
    repo.update(*task);
    CHECK(ids(repo.findByStatus(Status::Completed)) == Ids{"a"});
    CHECK(repo.countByStatus(Status::InProgress) == 0);
    CHECK(repo.findById(TaskId("a"))->completedAt().has_value());
    CHECK(repo.count() == 1);
}

void overdueFollowsEntityRule() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(0), kOpen, kLow, at(5)));

    // Passou do prazo e não está concluída: vencida.
    CHECK(ids(repo.findOverdue(at(10))) == Ids{"a"});

    // Depois de concluída, a regra da entidade a exclui, mesmo com o prazo vencido.
    auto task = repo.findById(TaskId("a"));
    task->start(at(1));
    task->complete(at(2));
    repo.update(*task);
    CHECK(repo.findOverdue(at(10)).empty());
}

void updatePriorityReindexes() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(0), kOpen, kLow));

    auto task = repo.findById(TaskId("a"));
    task->updatePriority(kHigh, at(1));
    repo.update(*task);

    CHECK(repo.findByPriority(kLow).empty());
    CHECK(ids(repo.findByPriority(kHigh)) == Ids{"a"});
}

void updateDeadlineReindexes() {
    InMemoryTaskRepository repo;
    repo.add(makeTask("a", at(0), kOpen, kLow, at(5)));

    // Move o deadline para mais tarde.
    auto task = repo.findById(TaskId("a"));
    task->updateDeadline(at(30), at(1));
    repo.update(*task);
    CHECK(repo.findByDeadlineBetween(at(0), at(10)).empty());
    CHECK(ids(repo.findByDeadlineBetween(at(20), at(40))) == Ids{"a"});

    // Remove o deadline: sai do índice de prazos e nunca fica vencida.
    task = repo.findById(TaskId("a"));
    task->updateDeadline(std::nullopt, at(2));
    repo.update(*task);
    CHECK(repo.findByDeadlineBetween(at(0), at(100)).empty());
    CHECK(repo.findOverdue(at(100)).empty());
    CHECK(repo.existsById(TaskId("a")));  // a tarefa continua no repositório
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------
int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"addAndFind", addAndFind},
        {"addDuplicateThrows", addDuplicateThrows},
        {"updateReplacesAndReindexes", updateReplacesAndReindexes},
        {"updateMissingThrows", updateMissingThrows},
        {"removeTask", removeTask},
        {"findAllOrderedByCreatedAt", findAllOrderedByCreatedAt},
        {"findByStatusAndCount", findByStatusAndCount},
        {"findByPriority", findByPriority},
        {"findByDeadlineBetween", findByDeadlineBetween},
        {"findOverdue", findOverdue},
        {"findByCreatedBetween", findByCreatedBetween},
        {"sameTimestampDoesNotCollide", sameTimestampDoesNotCollide},
        // Integração com Task
        {"storedCopyIsIsolated", storedCopyIsIsolated},
        {"lifecycleThroughRepository", lifecycleThroughRepository},
        {"overdueFollowsEntityRule", overdueFollowsEntityRule},
        {"updatePriorityReindexes", updatePriorityReindexes},
        {"updateDeadlineReindexes", updateDeadlineReindexes},
    };

    int failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[ OK ] " << name << "\n";
        } catch (const TestFailure& failure) {
            ++failed;
            std::cout << "[FAIL] " << name << "\n       " << failure.message << "\n";
        } catch (const std::exception& error) {
            ++failed;
            std::cout << "[FAIL] " << name << "\n       exceção inesperada: " << error.what() << "\n";
        }
    }

    std::cout << "\n" << (tests.size() - failed) << "/" << tests.size() << " testes passaram.\n";
    return failed == 0 ? 0 : 1;
}
