// Testes simples para Task.hpp / Task.cpp, sem dependências externas.
// Compilar: g++ -std=c++17 -Wall -Wextra Task.cpp Task_tests.cpp -o task_tests
// Executar: ./task_tests  (retorna 0 se tudo passar)

#include "src/Model/Entities/Task/Task.hpp"

#include <iostream>
#include <stdexcept>

using namespace Task;
using TimePoint = Task::Task::TimePoint;

// ---------------------------------------------------------------------------
// Mini framework de testes
// ---------------------------------------------------------------------------
namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "  FALHOU (" << __FILE__ << ":" << __LINE__ << "): "    \
                      << #cond << "\n";                                          \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr, ExceptionType)                                        \
    do {                                                                         \
        ++g_checks;                                                              \
        bool thrown = false;                                                     \
        try {                                                                    \
            expr;                                                                \
        } catch (const ExceptionType&) {                                         \
            thrown = true;                                                       \
        } catch (...) {                                                          \
        }                                                                        \
        if (!thrown) {                                                           \
            ++g_failures;                                                        \
            std::cerr << "  FALHOU (" << __FILE__ << ":" << __LINE__ << "): "    \
                      << #expr << " deveria lançar " << #ExceptionType << "\n";  \
        }                                                                        \
    } while (0)

// Tempo fixo para manter os testes determinísticos.
const TimePoint T0 = TimePoint{} + std::chrono::hours(1000);
TimePoint after(int hours) { return T0 + std::chrono::hours(hours); }

Task::Task makeTask(std::optional<TimePoint> deadline = std::nullopt) {
    return Task::Task::create(TaskId("t-1"), "Titulo", "Descricao", Priority::Medium, deadline, T0);
}

// ---------------------------------------------------------------------------
// TaskId
// ---------------------------------------------------------------------------
void test_task_id() {
    CHECK(TaskId("a") == TaskId("a"));
    CHECK(TaskId("a") != TaskId("b"));
    CHECK(TaskId("a") < TaskId("b"));
    CHECK(TaskId("abc").value() == "abc");
}

// ---------------------------------------------------------------------------
// create
// ---------------------------------------------------------------------------
void test_create_valid() {
    auto task = makeTask(after(5));

    CHECK(task.id() == TaskId("t-1"));
    CHECK(task.title() == "Titulo");
    CHECK(task.description() == "Descricao");
    CHECK(task.priority() == Priority::Medium);
    CHECK(task.status() == Status::Pending);
    CHECK(task.deadline().has_value() && *task.deadline() == after(5));
    CHECK(task.createdAt() == T0);
    CHECK(task.updatedAt() == T0);
    CHECK(!task.completedAt().has_value());
    CHECK(task.isPending());
    CHECK(!task.isCompleted());
}

void test_create_invalid() {
    CHECK_THROWS(Task::Task::create(TaskId("1"), "", "d", Priority::Low, std::nullopt, T0), std::invalid_argument);
    CHECK_THROWS(Task::Task::create(TaskId("1"), "   \t\n", "d", Priority::Low, std::nullopt, T0), std::invalid_argument);
    CHECK_THROWS(Task::Task::create(TaskId("1"), "ok", "d", Priority::Low, after(-1), T0), std::invalid_argument);
}

void test_create_deadline_equal_to_now_is_valid() {
    auto task = makeTask(T0);
    CHECK(task.deadline().has_value());
}

// ---------------------------------------------------------------------------
// Transições de estado
// ---------------------------------------------------------------------------
void test_start() {
    auto task = makeTask();
    task.start(after(1));

    CHECK(task.status() == Status::InProgress);
    CHECK(!task.isPending());
    CHECK(task.updatedAt() == after(1));
    CHECK(task.createdAt() == T0);

    CHECK_THROWS(task.start(after(2)), std::logic_error);  // já iniciada
}

void test_complete() {
    auto task = makeTask();
    task.start(after(1));
    task.complete(after(2));

    CHECK(task.status() == Status::Completed);
    CHECK(task.isCompleted());
    CHECK(task.completedAt().has_value() && *task.completedAt() == after(2));
    CHECK(task.updatedAt() == after(2));

    CHECK_THROWS(task.complete(after(3)), std::logic_error);  // já concluída
    CHECK_THROWS(task.start(after(3)), std::logic_error);     // não reinicia
}

void test_complete_directly_from_pending() {
    auto task = makeTask();
    task.complete(after(1));
    CHECK(task.isCompleted());
}

// ---------------------------------------------------------------------------
// Atualizações
// ---------------------------------------------------------------------------
void test_updates_valid() {
    auto task = makeTask();

    task.updateTitle("Novo", after(1));
    CHECK(task.title() == "Novo");
    CHECK(task.updatedAt() == after(1));

    task.updateDescription("Nova desc", after(2));
    CHECK(task.description() == "Nova desc");
    CHECK(task.updatedAt() == after(2));

    task.updatePriority(Priority::Critical, after(3));
    CHECK(task.priority() == Priority::Critical);
    CHECK(task.updatedAt() == after(3));

    task.updateDeadline(after(10), after(4));
    CHECK(task.deadline().has_value() && *task.deadline() == after(10));
    CHECK(task.updatedAt() == after(4));

    task.updateDeadline(std::nullopt, after(5));  // remover prazo
    CHECK(!task.deadline().has_value());
}

void test_updates_invalid_do_not_change_state() {
    auto task = makeTask(after(5));

    CHECK_THROWS(task.updateTitle("  ", after(1)), std::invalid_argument);
    CHECK(task.title() == "Titulo");

    CHECK_THROWS(task.updateDeadline(after(-5), after(1)), std::invalid_argument);
    CHECK(*task.deadline() == after(5));

    CHECK(task.updatedAt() == T0);
}

void test_updates_on_completed_task_throw() {
    auto task = makeTask();
    task.complete(after(1));

    CHECK_THROWS(task.updateTitle("x", after(2)), std::logic_error);
    CHECK_THROWS(task.updateDescription("x", after(2)), std::logic_error);
    CHECK_THROWS(task.updatePriority(Priority::High, after(2)), std::logic_error);
    CHECK_THROWS(task.updateDeadline(after(10), after(2)), std::logic_error);
    CHECK(task.updatedAt() == after(1));
}

// ---------------------------------------------------------------------------
// isOverdue
// ---------------------------------------------------------------------------
void test_is_overdue() {
    auto withDeadline = makeTask(after(5));
    CHECK(!withDeadline.isOverdue(after(4)));
    CHECK(!withDeadline.isOverdue(after(5)));  // exatamente no prazo
    CHECK(withDeadline.isOverdue(after(6)));

    auto noDeadline = makeTask();
    CHECK(!noDeadline.isOverdue(after(100)));

    auto completed = makeTask(after(5));
    completed.complete(after(1));
    CHECK(!completed.isOverdue(after(100)));  // concluída nunca está atrasada
}

struct TestCase {
    const char* name;
    void (*fn)();
};

}  // namespace

int main() {
    const TestCase tests[] = {
        {"task_id", test_task_id},
        {"create_valid", test_create_valid},
        {"create_invalid", test_create_invalid},
        {"create_deadline_equal_to_now_is_valid", test_create_deadline_equal_to_now_is_valid},
        {"start", test_start},
        {"complete", test_complete},
        {"complete_directly_from_pending", test_complete_directly_from_pending},
        {"updates_valid", test_updates_valid},
        {"updates_invalid_do_not_change_state", test_updates_invalid_do_not_change_state},
        {"updates_on_completed_task_throw", test_updates_on_completed_task_throw},
        {"is_overdue", test_is_overdue},
    };

    for (const auto& t : tests) {
        const int before = g_failures;
        t.fn();
        std::cout << (g_failures == before ? "[OK]    " : "[FALHA] ") << t.name << "\n";
    }

    std::cout << "\n" << g_checks << " verificações, " << g_failures << " falha(s)\n";
    return g_failures == 0 ? 0 : 1;
}
