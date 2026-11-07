#include "Task.hpp"

#include <algorithm>
#include <cctype>

namespace domain {

namespace {

bool isBlank(const std::string& text) noexcept {
    return std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c) != 0; });
}

void validateTitle(const std::string& title) {
    if (isBlank(title)) {
        throw std::invalid_argument("Task: title must not be empty");
    }
}

// O prazo não pode estar no passado em relação ao momento em que é definido.
void validateDeadline(const std::optional<Task::TimePoint>& deadline, Task::TimePoint now) {
    if (deadline && *deadline < now) {
        throw std::invalid_argument("Task: deadline must not be in the past");
    }
}

}  // namespace

Task::Task(TaskId id,
           std::string title,
           std::string description,
           Priority priority,
           std::optional<TimePoint> deadline,
           TimePoint createdAt)
    : id_(std::move(id)),
      title_(std::move(title)),
      description_(std::move(description)),
      priority_(priority),
      status_(Status::Pending),
      deadline_(deadline),
      createdAt_(createdAt),
      updatedAt_(createdAt),
      completedAt_(std::nullopt) {}

Task Task::create(std::string title,
                  std::string description,
                  Priority priority,
                  std::optional<TimePoint> deadline,
                  TaskId id,
                  TimePoint now) {
    validateTitle(title);
    validateDeadline(deadline, now);

    return Task(std::move(id), std::move(title), std::move(description), priority, deadline, now);
}

void Task::start(TimePoint now) {
    if (status_ != Status::Pending) {
        throw std::logic_error("Task: only a pending task can be started");
    }
    status_ = Status::InProgress;
    touch(now);
}

void Task::complete(TimePoint now) {
    if (isCompleted()) {
        throw std::logic_error("Task: task is already completed");
    }
    status_ = Status::Completed;
    completedAt_ = now;
    touch(now);
}

void Task::updateTitle(std::string newTitle, TimePoint now) {
    ensureNotCompleted("update title");
    validateTitle(newTitle);
    title_ = std::move(newTitle);
    touch(now);
}

void Task::updateDescription(std::string newDescription, TimePoint now) {
    ensureNotCompleted("update description");
    description_ = std::move(newDescription);
    touch(now);
}

void Task::updatePriority(Priority newPriority, TimePoint now) {
    ensureNotCompleted("update priority");
    priority_ = newPriority;
    touch(now);
}

void Task::updateDeadline(std::optional<TimePoint> newDeadline, TimePoint now) {
    ensureNotCompleted("update deadline");
    validateDeadline(newDeadline, now);
    deadline_ = newDeadline;
    touch(now);
}

bool Task::isCompleted() const noexcept {
    return status_ == Status::Completed;
}

bool Task::isPending() const noexcept {
    return status_ == Status::Pending;
}

bool Task::isOverdue(TimePoint now) const noexcept {
    return !isCompleted() && deadline_.has_value() && now > *deadline_;
}

void Task::touch(TimePoint now) noexcept {
    updatedAt_ = now;
}

void Task::ensureNotCompleted(const char* operation) const {
    if (isCompleted()) {
        throw std::logic_error(std::string("Task: cannot ") + operation + " of a completed task");
    }
}

}  // namespace domain
