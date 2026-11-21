#include "Project.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace Project {

namespace {

std::string trim(std::string_view value) {
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(whitespace);
    return std::string(value.substr(first, last - first + 1));
}

void validateId(ProjectId id) {
    if (id == Project::kInvalidId) {
        throw std::invalid_argument("Project: o identificador não pode ser zero.");
    }
}

std::string normalizeName(std::string_view rawName) {
    std::string name = trim(rawName);
    if (name.empty()) {
        throw std::invalid_argument("Project: o nome não pode ser vazio.");
    }
    if (name.size() > Project::kMaxNameLength) {
        throw std::invalid_argument("Project: o nome excede o tamanho máximo permitido.");
    }
    return name;
}

void validateDescription(const std::string& description) {
    if (description.size() > Project::kMaxDescriptionLength) {
        throw std::invalid_argument("Project: a descrição excede o tamanho máximo permitido.");
    }
}

void validateTaskId(const TaskId& taskId) {
    if (!taskId.isValid() || trim(taskId.value()).empty()) {
        throw std::invalid_argument("Project: o identificador da tarefa é inválido.");
    }
}

void validateDeadline(const std::optional<TimePoint>& deadline, TimePoint referenceTime) {
    if (deadline && *deadline < referenceTime) {
        throw std::invalid_argument("Project: o prazo não pode ser anterior à referência de tempo.");
    }
}

void validateTaskIdsList(const std::vector<TaskId>& taskIds) {
    std::set<TaskId> seen;
    for (const TaskId& taskId : taskIds) {
        validateTaskId(taskId);
        if (!seen.insert(taskId).second) {
            throw std::invalid_argument("Project: tarefa duplicada na lista de tarefas.");
        }
    }
}

}  // namespace

std::string_view toString(ProjectStatus status) noexcept {
    switch (status) {
        case ProjectStatus::Active:
            return "Active";
        case ProjectStatus::Completed:
            return "Completed";
        case ProjectStatus::Archived:
            return "Archived";
    }
    return "Unknown";
}

Project::Project(ProjectId id,
                 std::string name,
                 std::string description,
                 std::optional<TimePoint> deadline,
                 TimePoint createdAt)
    : id_(id),
      name_(),
      description_(),
      status_(ProjectStatus::Active),
      deadline_(),
      createdAt_(createdAt),
      updatedAt_(createdAt) {
    validateId(id);
    name_ = normalizeName(name);
    validateDescription(description);
    description_ = std::move(description);
    validateDeadline(deadline, createdAt);
    deadline_ = deadline;
}

Project Project::restore(ProjectId id,
                         std::string name,
                         std::string description,
                         ProjectStatus status,
                         std::optional<TimePoint> deadline,
                         TimePoint createdAt,
                         TimePoint updatedAt,
                         std::vector<TaskId> taskIds) {
    if (updatedAt < createdAt) {
        throw std::invalid_argument(
            "Project: a data de atualização não pode ser anterior à de criação.");
    }
    validateTaskIdsList(taskIds);

    Project project(id, std::move(name), std::move(description), deadline, createdAt);
    project.status_ = status;
    project.updatedAt_ = updatedAt;
    project.taskIds_ = std::move(taskIds);
    return project;
}

bool Project::hasTask(const TaskId& taskId) const noexcept {
    return std::find(taskIds_.begin(), taskIds_.end(), taskId) != taskIds_.end();
}

bool Project::isOverdue(TimePoint now) const noexcept {
    return status_ == ProjectStatus::Active && deadline_.has_value() && now > *deadline_;
}

void Project::rename(std::string newName, TimePoint now) {
    ensureNotArchived("renomear");
    name_ = normalizeName(newName);
    touch(now);
}

void Project::setDescription(std::string newDescription, TimePoint now) {
    ensureNotArchived("alterar a descrição");
    validateDescription(newDescription);
    description_ = std::move(newDescription);
    touch(now);
}

void Project::setDeadline(TimePoint deadline, TimePoint now) {
    ensureNotArchived("definir o prazo");
    validateDeadline(deadline, createdAt_);
    deadline_ = deadline;
    touch(now);
}

void Project::clearDeadline(TimePoint now) {
    ensureNotArchived("remover o prazo");
    if (deadline_.has_value()) {
        deadline_.reset();
        touch(now);
    }
}

bool Project::addTask(TaskId taskId, TimePoint now) {
    if (status_ != ProjectStatus::Active) {
        throw std::logic_error("Project: só é possível adicionar tarefas a um projeto ativo.");
    }
    validateTaskId(taskId);
    if (hasTask(taskId)) {
        return false;
    }
    taskIds_.push_back(std::move(taskId));
    touch(now);
    return true;
}

bool Project::removeTask(const TaskId& taskId, TimePoint now) {
    ensureNotArchived("remover tarefas");
    const auto it = std::find(taskIds_.begin(), taskIds_.end(), taskId);
    if (it == taskIds_.end()) {
        return false;
    }
    taskIds_.erase(it);
    touch(now);
    return true;
}

void Project::complete(TimePoint now) {
    if (status_ != ProjectStatus::Active) {
        throw std::logic_error("Project: só é possível concluir um projeto ativo.");
    }
    status_ = ProjectStatus::Completed;
    touch(now);
}

void Project::reopen(TimePoint now) {
    if (status_ != ProjectStatus::Completed) {
        throw std::logic_error("Project: só é possível reabrir um projeto concluído.");
    }
    status_ = ProjectStatus::Active;
    touch(now);
}

void Project::archive(TimePoint now) {
    if (status_ == ProjectStatus::Archived) {
        throw std::logic_error("Project: o projeto já está arquivado.");
    }
    status_ = ProjectStatus::Archived;
    touch(now);
}

void Project::unarchive(TimePoint now) {
    if (status_ != ProjectStatus::Archived) {
        throw std::logic_error("Project: só é possível desarquivar um projeto arquivado.");
    }
    status_ = ProjectStatus::Active;
    touch(now);
}

void Project::ensureNotArchived(std::string_view operation) const {
    if (status_ == ProjectStatus::Archived) {
        throw std::logic_error("Project: não é possível " + std::string(operation) +
                               " em um projeto arquivado.");
    }
}

void Project::touch(TimePoint now) noexcept {
    if (now > updatedAt_) {
        updatedAt_ = now;
    }
}

}  // namespace Project