#include "SchedulerService.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace SchedulerService {

// ---------------------------------------------------------------------------
// Ordenação
// ---------------------------------------------------------------------------
bool ScheduledTaskOrder::operator()(const ScheduledTask& lhs, const ScheduledTask& rhs) const noexcept {
    if (lhs.priority != rhs.priority) {
        return lhs.priority > rhs.priority;  // maior prioridade primeiro
    }

    const auto& lhsDeadline = lhs.task.deadline();
    const auto& rhsDeadline = rhs.task.deadline();
    if (lhsDeadline.has_value() && rhsDeadline.has_value()) {
        if (*lhsDeadline != *rhsDeadline) {
            return *lhsDeadline < *rhsDeadline;  // menor prazo primeiro
        }
    } else if (lhsDeadline.has_value() != rhsDeadline.has_value()) {
        return lhsDeadline.has_value();  // sem prazo vai para o final
    }

    return lhs.task.id() < rhs.task.id();  // menor id como desempate
}

// ---------------------------------------------------------------------------
// Auxiliares privados
// ---------------------------------------------------------------------------
void SchedulerService::insertEntry(Storage& storage, ScheduledTask entry) {
    const TaskId id = entry.task.id();
    if (storage.index.find(id) != storage.index.end()) {
        throw std::invalid_argument("SchedulerService: duplicated task id '" + id.value() + "'");
    }

    const auto [position, inserted] = storage.ordered.insert(std::move(entry));
    if (!inserted) {
        // Inalcançável enquanto o id for o último critério de desempate.
        throw std::logic_error("SchedulerService: ordering conflict for task id '" + id.value() + "'");
    }

    try {
        storage.index.emplace(id, position);
    } catch (...) {
        storage.ordered.erase(position);
        throw;
    }
}

SchedulerService::Storage SchedulerService::buildStorage(const std::vector<Task>& tasks,
                                                         const Storage* previous) {
    Storage storage;
    for (const Task& task : tasks) {
        if (task.isCompleted()) {
            continue;
        }

        Priority priority = task.priority();
        if (previous != nullptr) {
            const auto known = previous->index.find(task.id());
            if (known != previous->index.end()) {
                priority = known->second->priority;
            }
        }

        insertEntry(storage, ScheduledTask{task, priority});
    }
    return storage;
}

template <typename Predicate>
std::vector<ScheduledTask> SchedulerService::collect(Predicate predicate) const {
    std::vector<ScheduledTask> result;
    for (const ScheduledTask& entry : storage_.ordered) {
        if (predicate(entry)) {
            result.push_back(entry);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Construção e sincronização
// ---------------------------------------------------------------------------
void SchedulerService::buildSchedule(const std::vector<Task>& tasks) {
    if (!storage_.index.empty()) {
        throw std::logic_error("SchedulerService: schedule already built; use rebuildSchedule to replace it");
    }
    // Constrói fora do estado atual: se algo falhar, nada é alterado.
    storage_ = buildStorage(tasks, nullptr);
}

void SchedulerService::rebuildSchedule(const std::vector<Task>& tasks) {
    storage_ = buildStorage(tasks, nullptr);
}

void SchedulerService::refreshScheduler(const std::vector<Task>& currentTasks) {
    storage_ = buildStorage(currentTasks, &storage_);
}

// ---------------------------------------------------------------------------
// Consultas
// ---------------------------------------------------------------------------
std::optional<ScheduledTask> SchedulerService::getNextTask() const {
    for (const ScheduledTask& entry : storage_.ordered) {
        if (entry.task.isPending()) {
            return entry;
        }
    }
    return std::nullopt;
}

std::vector<ScheduledTask> SchedulerService::getTaskByPriority(Priority priority) const {
    return collect([priority](const ScheduledTask& entry) { return entry.priority == priority; });
}

std::vector<ScheduledTask> SchedulerService::getTaskByDeadline(TimePoint limit) const {
    return collect([limit](const ScheduledTask& entry) {
        const auto& deadline = entry.task.deadline();
        return deadline.has_value() && *deadline <= limit;
    });
}

std::vector<ScheduledTask> SchedulerService::getUrgentTasks(TimePoint now, Duration urgencyWindow) const {
    const TimePoint threshold = now + urgencyWindow;
    return collect([threshold](const ScheduledTask& entry) {
        if (entry.priority == Priority::Critical) {
            return true;
        }
        const auto& deadline = entry.task.deadline();
        return deadline.has_value() && *deadline <= threshold;
    });
}

std::vector<ScheduledTask> SchedulerService::getOverdueTasks(TimePoint now) const {
    return collect([now](const ScheduledTask& entry) { return entry.task.isOverdue(now); });
}

std::vector<ScheduledTask> SchedulerService::getAvaliableTasks() const {
    return collect([](const ScheduledTask& entry) { return entry.task.isPending(); });
}

// ---------------------------------------------------------------------------
// Modificações
// ---------------------------------------------------------------------------
bool SchedulerService::updateTaskPriority(const TaskId& id, Priority newPriority) {
    const auto found = storage_.index.find(id);
    if (found == storage_.index.end()) {
        return false;
    }

    const auto current = found->second;
    if (current->priority == newPriority) {
        return true;
    }

    // A chave de ordenação muda, então o elemento é reinserido. Inserir antes
    // de apagar mantém o agendamento intacto caso a inserção falhe.
    ScheduledTask updated{current->task, newPriority};
    const auto [position, inserted] = storage_.ordered.insert(std::move(updated));
    if (!inserted) {
        throw std::logic_error("SchedulerService: ordering conflict for task id '" + id.value() + "'");
    }

    storage_.ordered.erase(current);
    found->second = position;
    return true;
}

bool SchedulerService::removeTaskFromScheduler(const TaskId& id) {
    const auto found = storage_.index.find(id);
    if (found == storage_.index.end()) {
        return false;
    }

    storage_.ordered.erase(found->second);
    storage_.index.erase(found);
    return true;
}

}  // namespace SchedulerService
