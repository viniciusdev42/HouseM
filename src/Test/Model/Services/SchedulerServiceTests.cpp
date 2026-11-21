#include <chrono>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "src/Main/Model/Services/Scheduler/SchedulerService.hpp"

namespace {

using TaskEntity = ::Task::Task;
using ::Task::Priority;
using ::Task::TaskId;
using Repository = ::TaskRepository::InMemoryTaskRepository;
using Scheduler = ::SchedulerService::SchedulerService;
using ::SchedulerService::ScheduledTask;
using TimePoint = TaskEntity::TimePoint;

// ----------------------------- mini framework ------------------------------
int g_failures = 0;
int g_checks = 0;

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
        bool thrown_ = false;                                                    \
        try {                                                                    \
            expr;                                                                \
        } catch (const ExceptionType&) {                                         \
            thrown_ = true;                                                      \
        } catch (...) {                                                          \
        }                                                                        \
        if (!thrown_) {                                                          \
            ++g_failures;                                                        \
            std::cerr << "  FALHOU (linha " << __LINE__                          \
                      << "): esperava " #ExceptionType " em " #expr "\n";        \
        }                                                                        \
    } while (0)

void runTest(const char* name, void (*fn)()) {
    const int before = g_failures;
    try {
        fn();
    } catch (const std::exception& e) {
        ++g_failures;
        std::cerr << "  EXCECAO inesperada: " << e.what() << "\n";
    }
    std::cout << (g_failures == before ? "[OK]    " : "[FALHA] ") << name << "\n";
}

// ------------------------------- auxiliares --------------------------------
const TimePoint kBase = TimePoint{} + std::chrono::hours(24 * 365 * 40);  // instante fixo
constexpr auto kHour = std::chrono::hours(1);

TaskEntity makeTask(const std::string& id, Priority priority,
                    std::optional<int> deadlineHours = std::nullopt) {
    std::optional<TimePoint> deadline;
    if (deadlineHours) {
        deadline = kBase + std::chrono::hours(*deadlineHours);
    }
    return TaskEntity::create(TaskId{id}, "titulo " + id, "descricao", priority, deadline, kBase);
}

TaskEntity makeCompleted(const std::string& id) {
    TaskEntity task = makeTask(id, Priority::Medium);
    task.start(kBase);
    task.complete(kBase);
    return task;
}

std::vector<std::string> ids(const std::vector<ScheduledTask>& tasks) {
    std::vector<std::string> result;
    for (const auto& t : tasks) {
        result.push_back(t.task.id().value());
    }
    return result;
}

using Ids = std::vector<std::string>;

// --------------------------------- testes ----------------------------------
void testBuildIgnoresCompletedAndRejectsSecondBuild() {
    Repository repo;
    repo.add(makeTask("a", Priority::Low));
    repo.add(makeCompleted("done"));

    Scheduler scheduler{repo};
    CHECK(scheduler.empty());
    scheduler.buildSchedule();

    CHECK(scheduler.size() == 1);
    CHECK(ids(scheduler.getAvaliableTasks()) == Ids{"a"});
    CHECK_THROWS(scheduler.buildSchedule(), std::logic_error);
}

void testOrdering() {
    Repository repo;
    repo.add(makeTask("low", Priority::Low, 1));
    repo.add(makeTask("high-late", Priority::High, 10));
    repo.add(makeTask("high-early", Priority::High, 2));
    repo.add(makeTask("high-none", Priority::High));
    repo.add(makeTask("crit-b", Priority::Critical, 5));
    repo.add(makeTask("crit-a", Priority::Critical, 5));

    Scheduler scheduler{repo};
    scheduler.buildSchedule();

    // prioridade desc -> prazo asc (sem prazo por ultimo) -> id asc
    const Ids expected{"crit-a", "crit-b", "high-early", "high-late", "high-none", "low"};
    CHECK(ids(scheduler.getAvaliableTasks()) == expected);
}

void testGetNextTask() {
    Repository repo;
    Scheduler scheduler{repo};
    scheduler.buildSchedule();
    CHECK(!scheduler.getNextTask().has_value());

    repo.add(makeTask("low", Priority::Low));
    repo.add(makeTask("high", Priority::High));
    scheduler.refreshScheduler();

    const auto next = scheduler.getNextTask();
    CHECK(next.has_value());
    CHECK(next->task.id().value() == "high");
}

void testQueries() {
    Repository repo;
    repo.add(makeTask("low", Priority::Low, 100));
    repo.add(makeTask("med", Priority::Medium, 10));
    repo.add(makeTask("high", Priority::High));
    repo.add(makeTask("crit", Priority::Critical, 500));

    Scheduler scheduler{repo};
    scheduler.buildSchedule();

    // por prioridade
    CHECK(ids(scheduler.getTaskByPriority(Priority::Medium)) == Ids{"med"});
    CHECK(scheduler.getTaskByPriority(Priority::High).size() == 1);

    // por prazo (<= limite; sem prazo nunca entra)
    CHECK(ids(scheduler.getTaskByDeadline(kBase + 10 * kHour)) == Ids{"med"});
    CHECK(ids(scheduler.getTaskByDeadline(kBase + 100 * kHour)) == Ids{"med", "low"});

    // urgentes: Critical OU prazo <= now + janela
    const auto urgent = ids(scheduler.getUrgentTasks(kBase, 24 * kHour));
    CHECK(urgent == Ids{"crit", "med"});

    // atrasadas
    CHECK(ids(scheduler.getOverdueTasks(kBase + 50 * kHour)) == Ids{"med"});
    CHECK(scheduler.getOverdueTasks(kBase).empty());
}

void testUpdateTaskPriority() {
    Repository repo;
    repo.add(makeTask("a", Priority::Low));
    repo.add(makeTask("b", Priority::High));

    Scheduler scheduler{repo};
    scheduler.buildSchedule();
    CHECK(scheduler.getNextTask()->task.id().value() == "b");

    CHECK(scheduler.updateTaskPriority(TaskId{"a"}, Priority::Critical));
    CHECK(scheduler.getNextTask()->task.id().value() == "a");
    CHECK(scheduler.getNextTask()->priority == Priority::Critical);
    CHECK(scheduler.size() == 2);

    // mesma prioridade: ok; id inexistente: false
    CHECK(scheduler.updateTaskPriority(TaskId{"a"}, Priority::Critical));
    CHECK(!scheduler.updateTaskPriority(TaskId{"nope"}, Priority::Low));

    // a Task e o repositorio nao mudam
    CHECK(repo.findById(TaskId{"a"})->priority() == Priority::Low);
}

void testRemoveTaskFromScheduler() {
    Repository repo;
    repo.add(makeTask("a", Priority::Low));
    repo.add(makeTask("b", Priority::High));

    Scheduler scheduler{repo};
    scheduler.buildSchedule();

    CHECK(scheduler.removeTaskFromScheduler(TaskId{"b"}));
    CHECK(!scheduler.removeTaskFromScheduler(TaskId{"b"}));
    CHECK(scheduler.size() == 1);
    CHECK(scheduler.getNextTask()->task.id().value() == "a");
    CHECK(repo.existsById(TaskId{"b"}));  // repositorio intacto
}

void testRefreshPreservesPriorityAndSyncs() {
    Repository repo;
    repo.add(makeTask("a", Priority::Low));
    repo.add(makeTask("b", Priority::Low));
    repo.add(makeTask("c", Priority::Low));

    Scheduler scheduler{repo};
    scheduler.buildSchedule();
    CHECK(scheduler.updateTaskPriority(TaskId{"a"}, Priority::Critical));

    repo.remove(TaskId{"b"});                  // sumiu
    repo.update(makeCompleted("c"));           // concluida
    repo.add(makeTask("d", Priority::Medium)); // nova

    scheduler.refreshScheduler();

    CHECK(ids(scheduler.getAvaliableTasks()) == Ids{"a", "d"});
    CHECK(scheduler.getNextTask()->priority == Priority::Critical);  // preservada
    CHECK(scheduler.size() == 2);
}

void testRebuildDiscardsPriorityChanges() {
    Repository repo;
    repo.add(makeTask("a", Priority::Low));
    repo.add(makeTask("b", Priority::High));

    Scheduler scheduler{repo};
    scheduler.buildSchedule();
    CHECK(scheduler.updateTaskPriority(TaskId{"a"}, Priority::Critical));

    scheduler.rebuildSchedule();

    CHECK(scheduler.getNextTask()->task.id().value() == "b");
    CHECK(scheduler.getTaskByPriority(Priority::Low).size() == 1);
}

}  // namespace

int main() {
    runTest("build ignora concluidas e rejeita segundo build", testBuildIgnoresCompletedAndRejectsSecondBuild);
    runTest("ordenacao (prioridade, prazo, id)", testOrdering);
    runTest("getNextTask", testGetNextTask);
    runTest("consultas (prioridade, prazo, urgentes, atrasadas)", testQueries);
    runTest("updateTaskPriority", testUpdateTaskPriority);
    runTest("removeTaskFromScheduler", testRemoveTaskFromScheduler);
    runTest("refreshScheduler preserva prioridades e sincroniza", testRefreshPreservesPriorityAndSyncs);
    runTest("rebuildSchedule descarta prioridades alteradas", testRebuildDiscardsPriorityChanges);

    std::cout << "\n" << g_checks << " verificacoes, " << g_failures << " falha(s)\n";
    return g_failures == 0 ? 0 : 1;
}
