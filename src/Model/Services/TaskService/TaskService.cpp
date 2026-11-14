#include "TaskService.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace TaskService {

TaskService::TaskService(std::shared_ptr<Task::ITaskRepository> repository,
                         NowProvider nowProvider)
    : repository_(std::move(repository)), nowProvider_(std::move(nowProvider)) {
    if (!repository_) {
        throw std::invalid_argument("TaskService: repository must not be null");
    }
    if (!nowProvider_) {
        throw std::invalid_argument("TaskService: nowProvider must not be empty");
    }
}

// ----------------------------- Criação -------------------------------------

Task::Task TaskService::createTask(std::string title,
                                     std::string description,
                                     Task::Priority priority,
                                     std::optional<TimePoint> deadline) {
    Task::Task task = Task::Task::create(std::move(title),
                                             std::move(description),
                                             priority,
                                             deadline,
                                             Task::TaskId::generate(),
                                             nowProvider_());
    repository_->save(task);
    return task;
}

// ------------------------- Transições de estado ----------------------------

Task::Task TaskService::startTask(const Task::TaskId& id) {
    return modifyTask(id, [](Task::Task& task, TimePoint now) { task.start(now); });
}

Task::Task TaskService::completeTask(const Task::TaskId& id) {
    return modifyTask(id, [](Task::Task& task, TimePoint now) { task.complete(now); });
}

// ----------------------------- Atualizações --------------------------------

Task::Task TaskService::updateTaskTitle(const Task::TaskId& id, std::string newTitle) {
    return modifyTask(id, [&newTitle](Task::Task& task, TimePoint now) {
        task.updateTitle(std::move(newTitle), now);
    });
}

Task::Task TaskService::updateTaskDescription(const Task::TaskId& id, std::string newDescription) {
    return modifyTask(id, [&newDescription](Task::Task& task, TimePoint now) {
        task.updateDescription(std::move(newDescription), now);
    });
}

Task::Task TaskService::updateTaskPriority(const Task::TaskId& id, Task::Priority newPriority) {
    return modifyTask(id, [newPriority](Task::Task& task, TimePoint now) {
        task.updatePriority(newPriority, now);
    });
}

Task::Task TaskService::updateTaskDeadline(const Task::TaskId& id,
                                             std::optional<TimePoint> newDeadline) {
    return modifyTask(id, [newDeadline](Task::Task& task, TimePoint now) {
        task.updateDeadline(newDeadline, now);
    });
}

// ------------------------------- Consultas ---------------------------------

std::optional<Task::Task> TaskService::getTaskById(const Task::TaskId& id) const {
    return repository_->findById(id);
}

std::vector<Task::Task> TaskService::listAllTasks() const {
    return repository_->findAll();
}

std::vector<Task::Task> TaskService::listPendingTasks() const {
    return listWhere([](const Task::Task& task) { return task.isPending(); });
}

std::vector<Task::Task> TaskService::listCompletedTasks() const {
    return listWhere([](const Task::Task& task) { return task.isCompleted(); });
}

std::vector<Task::Task> TaskService::listOverdueTasks() const {
    // O "agora" é capturado uma única vez para que todas as tarefas
    // sejam avaliadas contra o mesmo instante.
    const TimePoint now = nowProvider_();
    return listWhere([now](const Task::Task& task) { return task.isOverdue(now); });
}

// ------------------------------- Privados ----------------------------------

Task::Task TaskService::modifyTask(const Task::TaskId& id, const Mutation& mutation) {
    std::optional<Task::Task> found = repository_->findById(id);
    if (!found) {
        throw TaskNotFoundException(id);
    }

    Task::Task task = std::move(*found);
    mutation(task, nowProvider_());
    repository_->save(task);
    return task;
}

std::vector<Task::Task> TaskService::listWhere(
    const std::function<bool(const Task::Task&)>& predicate) const {
    std::vector<Task::Task> all = repository_->findAll();
    std::vector<Task::Task> result;
    result.reserve(all.size());
    std::copy_if(std::make_move_iterator(all.begin()),
                 std::make_move_iterator(all.end()),
                 std::back_inserter(result),
                 predicate);
    return result;
}

}  // namespace TaskService
