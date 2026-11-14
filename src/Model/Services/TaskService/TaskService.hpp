#ifndef TASK_SERVICE
#define TASK_SERVICE

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/Model/Repositories/ITaskRepository.hpp"
#include "src/Model/Entities/Task/Task.hpp"

namespace TaskService {

// Lançada quando uma operação exige uma tarefa que não existe no repositório.
class TaskNotFoundException : public std::runtime_error {
public:
    explicit TaskNotFoundException(const Task::TaskId& id)
        : std::runtime_error("Task not found: " + id.value()) {}
};

// ---------------------------------------------------------------------------
// TaskService: Serviço de aplicação
// Orquestra os casos de uso sobre Task, delegando a persistência a uma
// abstração (ITaskRepository) e as regras de negócio à própria entidade.
// O relógio é injetável para tornar o comportamento determinístico.
// ---------------------------------------------------------------------------
class TaskService {
public:
    using Clock = Task::Task::Clock;
    using TimePoint = Task::Task::TimePoint;
    using NowProvider = std::function<TimePoint()>;

    // Lança std::invalid_argument se o repositório for nulo.
    explicit TaskService(std::shared_ptr<Task::ITaskRepository> repository,
                         NowProvider nowProvider = [] { return Clock::now(); });

    // Criação
    Task::Task createTask(std::string title,
                            std::string description,
                            Task::Priority priority,
                            std::optional<TimePoint> deadline = std::nullopt);

    // Transições de estado
    Task::Task startTask(const Task::TaskId& id);
    Task::Task completeTask(const Task::TaskId& id);

    // Atualizações
    Task::Task updateTaskTitle(const Task::TaskId& id, std::string newTitle);
    Task::Task updateTaskDescription(const Task::TaskId& id, std::string newDescription);
    Task::Task updateTaskPriority(const Task::TaskId& id, Task::Priority newPriority);
    Task::Task updateTaskDeadline(const Task::TaskId& id, std::optional<TimePoint> newDeadline);

    // Consultas
    std::optional<Task::Task> getTaskById(const Task::TaskId& id) const;
    std::vector<Task::Task> listAllTasks() const;
    std::vector<Task::Task> listPendingTasks() const;
    std::vector<Task::Task> listCompletedTasks() const;
    std::vector<Task::Task> listOverdueTasks() const;

private:
    using Mutation = std::function<void(Task::Task&, TimePoint)>;

    // Carrega a tarefa, aplica a mutação, persiste e retorna o resultado.
    Task::Task modifyTask(const Task::TaskId& id, const Mutation& mutation);

    std::vector<Task::Task> listWhere(const std::function<bool(const Task::Task&)>& predicate) const;

    std::shared_ptr<Task::ITaskRepository> repository_;
    NowProvider nowProvider_;
};

}  // namespace TaskService

#endif // TASK_SERVICE
