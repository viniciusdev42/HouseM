#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/Main/Model/Services/Task/TaskService.hpp"

// ---------------------------------------------------------------------------
// Mini-harness de testes
// ---------------------------------------------------------------------------
static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "  FALHA (" << __FILE__ << ":" << __LINE__ << "): "     \
                      << #cond << "\n";                                          \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr, ExType)                                               \
    do {                                                                         \
        ++g_checks;                                                              \
        bool thrown = false;                                                     \
        try { (void)(expr); } catch (const ExType&) { thrown = true; }           \
        catch (...) {}                                                           \
        if (!thrown) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "  FALHA (" << __FILE__ << ":" << __LINE__ << "): "     \
                      << #expr << " deveria lançar " << #ExType << "\n";         \
        }                                                                        \
    } while (0)

static void runTest(const std::string& name, const std::function<void()>& test) {
    const int before = g_failures;
    try {
        test();
    } catch (const std::exception& e) {
        ++g_failures;
        std::cerr << "  EXCEÇÃO inesperada: " << e.what() << "\n";
    }
    std::cout << (g_failures == before ? "[ OK ] " : "[FAIL] ") << name << "\n";
}

// ---------------------------------------------------------------------------
// Dublê de teste: repositório em memória mínimo
// ---------------------------------------------------------------------------
using TS = TaskService::TaskService;
using TimePoint = TS::TimePoint;

class FakeRepository : public Task::ITaskRepository {
public:
    void save(const Task::Task& task) override {
        ++saveCount;
        tasks_.insert_or_assign(task.id().value(), task);
    }

    std::optional<Task::Task> findById(const Task::TaskId& id) const override {
        const auto it = tasks_.find(id.value());
        if (it == tasks_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<Task::Task> findAll() const override {
        std::vector<Task::Task> all;
        for (const auto& entry : tasks_) all.push_back(entry.second);
        return all;
    }

    int saveCount = 0;

private:
    std::map<std::string, Task::Task> tasks_;
};

// Ambiente de teste: repositório falso + relógio controlável + serviço.
struct Env {
    std::shared_ptr<FakeRepository> repo = std::make_shared<FakeRepository>();
    TimePoint now = TimePoint{} + std::chrono::hours(1000);
    TS service{repo, [this] { return now; }};

    void advance(std::chrono::hours h) { now += h; }

    Task::Task create(const std::string& id,
                      Task::Priority priority = Task::Priority::Medium,
                      std::optional<TimePoint> deadline = std::nullopt) {
        return service.createTask(Task::TaskId{id}, "Titulo " + id, "Descricao", priority, deadline);
    }
};

// ---------------------------------------------------------------------------
// Testes
// ---------------------------------------------------------------------------
static void testConstructorRejectsInvalidArguments() {
    CHECK_THROWS(TS(nullptr), std::invalid_argument);
    CHECK_THROWS(TS(std::make_shared<FakeRepository>(), TS::NowProvider{}), std::invalid_argument);
}

static void testCreateTaskPersistsAndReturns() {
    Env env;
    const Task::Task task = env.create("t1", Task::Priority::High);

    CHECK(task.id() == Task::TaskId{"t1"});
    CHECK(task.title() == "Titulo t1");
    CHECK(task.priority() == Task::Priority::High);
    CHECK(task.status() == Task::Status::Pending);
    CHECK(task.createdAt() == env.now);
    CHECK(env.repo->findById(Task::TaskId{"t1"}).has_value());
}

static void testCreateTaskWithInvalidDataDoesNotPersist() {
    Env env;
    const TimePoint past = env.now - std::chrono::hours(1);

    CHECK_THROWS(env.service.createTask(Task::TaskId{"a"}, "   ", "d", Task::Priority::Low),
                 std::invalid_argument);
    CHECK_THROWS(env.service.createTask(Task::TaskId{"b"}, "ok", "d", Task::Priority::Low, past),
                 std::invalid_argument);
    CHECK(env.repo->saveCount == 0);
}

static void testStartTask() {
    Env env;
    env.create("t1");
    env.advance(std::chrono::hours(1));

    const Task::Task started = env.service.startTask(Task::TaskId{"t1"});

    CHECK(started.status() == Task::Status::InProgress);
    CHECK(started.updatedAt() == env.now);
    CHECK(env.service.getTaskById(Task::TaskId{"t1"})->status() == Task::Status::InProgress);
    // Só tarefas pendentes podem ser iniciadas.
    CHECK_THROWS(env.service.startTask(Task::TaskId{"t1"}), std::logic_error);
}

static void testCompleteTask() {
    Env env;
    env.create("t1");
    env.advance(std::chrono::hours(2));

    const Task::Task done = env.service.completeTask(Task::TaskId{"t1"});

    CHECK(done.isCompleted());
    CHECK(done.completedAt().has_value() && *done.completedAt() == env.now);
    CHECK(env.service.getTaskById(Task::TaskId{"t1"})->isCompleted());
    CHECK_THROWS(env.service.completeTask(Task::TaskId{"t1"}), std::logic_error);
}

static void testOperationsOnMissingTaskThrowNotFound() {
    Env env;
    const Task::TaskId missing{"nao-existe"};

    CHECK_THROWS(env.service.startTask(missing), TaskService::TaskNotFoundException);
    CHECK_THROWS(env.service.completeTask(missing), TaskService::TaskNotFoundException);
    CHECK_THROWS(env.service.updateTaskTitle(missing, "x"), TaskService::TaskNotFoundException);
    CHECK_THROWS(env.service.updateTaskDescription(missing, "x"), TaskService::TaskNotFoundException);
    CHECK_THROWS(env.service.updateTaskPriority(missing, Task::Priority::Low),
                 TaskService::TaskNotFoundException);
    CHECK_THROWS(env.service.updateTaskDeadline(missing, std::nullopt),
                 TaskService::TaskNotFoundException);
}

static void testUpdateTaskFields() {
    Env env;
    env.create("t1");
    const Task::TaskId id{"t1"};
    env.advance(std::chrono::hours(1));

    CHECK(env.service.updateTaskTitle(id, "Novo titulo").title() == "Novo titulo");
    CHECK(env.service.updateTaskDescription(id, "Nova desc").description() == "Nova desc");
    CHECK(env.service.updateTaskPriority(id, Task::Priority::Critical).priority() ==
          Task::Priority::Critical);

    const TimePoint deadline = env.now + std::chrono::hours(5);
    const Task::Task updated = env.service.updateTaskDeadline(id, deadline);
    CHECK(updated.deadline().has_value() && *updated.deadline() == deadline);
    CHECK(updated.updatedAt() == env.now);

    // Tudo foi persistido.
    const auto stored = env.service.getTaskById(id);
    CHECK(stored.has_value());
    CHECK(stored->title() == "Novo titulo");
    CHECK(stored->priority() == Task::Priority::Critical);
}

static void testUpdateWithInvalidDataOrCompletedTaskThrows() {
    Env env;
    env.create("t1");
    const Task::TaskId id{"t1"};

    CHECK_THROWS(env.service.updateTaskTitle(id, ""), std::invalid_argument);
    CHECK_THROWS(env.service.updateTaskDeadline(id, env.now - std::chrono::hours(1)),
                 std::invalid_argument);

    env.service.completeTask(id);
    CHECK_THROWS(env.service.updateTaskTitle(id, "x"), std::logic_error);
    CHECK_THROWS(env.service.updateTaskPriority(id, Task::Priority::Low), std::logic_error);
}

static void testGetTaskById() {
    Env env;
    env.create("t1");

    CHECK(env.service.getTaskById(Task::TaskId{"t1"}).has_value());
    CHECK(!env.service.getTaskById(Task::TaskId{"outra"}).has_value());
}

static void testListAllTasks() {
    Env env;
    CHECK(env.service.listAllTasks().empty());

    env.create("t1");
    env.create("t2");
    CHECK(env.service.listAllTasks().size() == 2);
}

static void testListPendingAndCompletedTasks() {
    Env env;
    env.create("pendente");
    env.create("em-andamento");
    env.create("concluida");
    env.service.startTask(Task::TaskId{"em-andamento"});
    env.service.completeTask(Task::TaskId{"concluida"});

    const auto pending = env.service.listPendingTasks();
    CHECK(pending.size() == 1);
    CHECK(pending.size() == 1 && pending[0].id() == Task::TaskId{"pendente"});

    const auto completed = env.service.listCompletedTasks();
    CHECK(completed.size() == 1);
    CHECK(completed.size() == 1 && completed[0].id() == Task::TaskId{"concluida"});
}

static void testListOverdueTasks() {
    Env env;
    const TimePoint deadline = env.now + std::chrono::hours(1);

    env.create("vencida", Task::Priority::Medium, deadline);
    env.create("concluida", Task::Priority::Medium, deadline);
    env.create("sem-prazo");
    env.create("no-prazo", Task::Priority::Medium, env.now + std::chrono::hours(10));
    env.service.completeTask(Task::TaskId{"concluida"});

    CHECK(env.service.listOverdueTasks().empty());  // ainda dentro do prazo

    env.advance(std::chrono::hours(2));  // passa o prazo de "vencida"

    const auto overdue = env.service.listOverdueTasks();
    CHECK(overdue.size() == 1);
    CHECK(overdue.size() == 1 && overdue[0].id() == Task::TaskId{"vencida"});
}

// ---------------------------------------------------------------------------
int main() {
    runTest("construtor rejeita argumentos invalidos", testConstructorRejectsInvalidArguments);
    runTest("createTask persiste e retorna a tarefa", testCreateTaskPersistsAndReturns);
    runTest("createTask com dados invalidos nao persiste", testCreateTaskWithInvalidDataDoesNotPersist);
    runTest("startTask", testStartTask);
    runTest("completeTask", testCompleteTask);
    runTest("operacoes em tarefa inexistente lancam TaskNotFoundException",
            testOperationsOnMissingTaskThrowNotFound);
    runTest("updateTask* atualizam e persistem", testUpdateTaskFields);
    runTest("updateTask* com dados invalidos ou tarefa concluida lancam",
            testUpdateWithInvalidDataOrCompletedTaskThrows);
    runTest("getTaskById", testGetTaskById);
    runTest("listAllTasks", testListAllTasks);
    runTest("listPendingTasks / listCompletedTasks", testListPendingAndCompletedTasks);
    runTest("listOverdueTasks", testListOverdueTasks);

    std::cout << "\n" << g_checks << " verificacoes, " << g_failures << " falha(s)\n";
    return g_failures == 0 ? 0 : 1;
}
