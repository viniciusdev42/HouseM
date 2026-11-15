#ifndef TASK_HPP
#define TASK_HPP

#include <array>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace Task {

// ---------------------------------------------------------------------------
// TaskId: Value Object
// ---------------------------------------------------------------------------
class TaskId {
public:
    explicit TaskId(std::string value): value_(std::move(value)) {}

    const std::string& value() const noexcept { return value_; }

    friend bool operator==(const TaskId& lhs, const TaskId& rhs) noexcept { return lhs.value_ == rhs.value_; }
    friend bool operator!=(const TaskId& lhs, const TaskId& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator<(const TaskId& lhs, const TaskId& rhs) noexcept { return lhs.value_ < rhs.value_; }

private:
    std::string value_;
};

enum class Priority {
    Low,
    Medium,
    High,
    Critical
};

enum class Status {
    Pending,
    InProgress,
    Completed
};

// ---------------------------------------------------------------------------
// Task: Entidade de domínio
// Instâncias só podem ser criadas por Task::create, garantindo invariantes.
// Os métodos que dependem do tempo aceitam um "now" opcional, o que facilita
// o determinismo sem acoplar a entidade a uma abstração de relógio.
// ---------------------------------------------------------------------------
class Task {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    static Task create(TaskId id,
                       std::string title,
                       std::string description,
                       Priority priority,
                       std::optional<TimePoint> deadline = std::nullopt,                       
                       TimePoint now = Clock::now());

    // Transições de estado
    void start(TimePoint now = Clock::now());
    void complete(TimePoint now = Clock::now());

    // Atualizações
    void updateTitle(std::string newTitle, TimePoint now = Clock::now());
    void updateDescription(std::string newDescription, TimePoint now = Clock::now());
    void updatePriority(Priority newPriority, TimePoint now = Clock::now());
    void updateDeadline(std::optional<TimePoint> newDeadline, TimePoint now = Clock::now());

    // Consultas
    bool isCompleted() const noexcept;
    bool isPending() const noexcept;
    bool isOverdue(TimePoint now = Clock::now()) const noexcept;

    // Acesso aos dados
    const TaskId& id() const noexcept { return id_; }
    const std::string& title() const noexcept { return title_; }
    const std::string& description() const noexcept { return description_; }
    Priority priority() const noexcept { return priority_; }
    Status status() const noexcept { return status_; }
    const std::optional<TimePoint>& deadline() const noexcept { return deadline_; }
    TimePoint createdAt() const noexcept { return createdAt_; }
    TimePoint updatedAt() const noexcept { return updatedAt_; }
    const std::optional<TimePoint>& completedAt() const noexcept { return completedAt_; }

private:
    Task(TaskId id,
         std::string title,
         std::string description,
         Priority priority,
         std::optional<TimePoint> deadline,
         TimePoint createdAt);

    void touch(TimePoint now) noexcept;
    void ensureNotCompleted(const char* operation) const;

    TaskId id_;
    std::string title_;
    std::string description_;
    Priority priority_;
    Status status_;
    std::optional<TimePoint> deadline_;
    TimePoint createdAt_;
    TimePoint updatedAt_;
    std::optional<TimePoint> completedAt_;
};

}  // namespace Task
#endif // TASK_HPP
