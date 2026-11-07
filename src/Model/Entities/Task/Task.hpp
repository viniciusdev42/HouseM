#pragma once

#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace domain {

// ---------------------------------------------------------------------------
// TaskId: Value Object
// Imutável, comparável por valor e sempre válido (formato UUID 8-4-4-4-12).
// ---------------------------------------------------------------------------
class TaskId {
public:
    // Gera um novo identificador aleatório (UUID v4).
    static TaskId generate() {
        thread_local std::mt19937_64 rng{std::random_device{}()};

        std::uint64_t high = rng();
        std::uint64_t low = rng();

        high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;  // versão 4
        low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;    // variante RFC 4122

        std::ostringstream out;
        out << std::hex << std::setfill('0')
            << std::setw(8) << (high >> 32) << '-'
            << std::setw(4) << ((high >> 16) & 0xFFFFULL) << '-'
            << std::setw(4) << (high & 0xFFFFULL) << '-'
            << std::setw(4) << (low >> 48) << '-'
            << std::setw(12) << (low & 0xFFFFFFFFFFFFULL);

        return TaskId(out.str());
    }

    // Reconstrói um identificador existente, validando o formato.
    static TaskId from(std::string value) {
        if (!isValidFormat(value)) {
            throw std::invalid_argument("TaskId: invalid format, expected UUID (8-4-4-4-12): '" + value + "'");
        }
        for (char& c : value) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return TaskId(std::move(value));
    }

    const std::string& value() const noexcept { return value_; }

    friend bool operator==(const TaskId& lhs, const TaskId& rhs) noexcept { return lhs.value_ == rhs.value_; }
    friend bool operator!=(const TaskId& lhs, const TaskId& rhs) noexcept { return !(lhs == rhs); }
    friend bool operator<(const TaskId& lhs, const TaskId& rhs) noexcept { return lhs.value_ < rhs.value_; }

private:
    explicit TaskId(std::string value) : value_(std::move(value)) {}

    static bool isValidFormat(const std::string& candidate) noexcept {
        constexpr std::size_t kLength = 36;
        constexpr std::array<std::size_t, 4> kHyphenPositions{8, 13, 18, 23};

        if (candidate.size() != kLength) {
            return false;
        }
        for (std::size_t i = 0; i < kLength; ++i) {
            const bool isHyphenPosition =
                (i == kHyphenPositions[0] || i == kHyphenPositions[1] ||
                 i == kHyphenPositions[2] || i == kHyphenPositions[3]);
            if (isHyphenPosition) {
                if (candidate[i] != '-') {
                    return false;
                }
            } else if (!std::isxdigit(static_cast<unsigned char>(candidate[i]))) {
                return false;
            }
        }
        return true;
    }

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

    static Task create(std::string title,
                       std::string description,
                       Priority priority,
                       std::optional<TimePoint> deadline = std::nullopt,
                       TaskId id = TaskId::generate(),
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

}  // namespace domain
