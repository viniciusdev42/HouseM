#ifndef TASK_REPOSITORY
#define TASK_REPOSITORY

#include <array>
#include <cstddef>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Task.hpp"

namespace ITaskRepository {

// ---------------------------------------------------------------------------
// Erros do repositório
// ---------------------------------------------------------------------------
class TaskNotFoundError : public std::runtime_error {
public:
    explicit TaskNotFoundError(const TaskId& id)
        : std::runtime_error("TaskRepository: task not found: '" + id.value() + "'") {}
};

class DuplicateTaskError : public std::runtime_error {
public:
    explicit DuplicateTaskError(const TaskId& id)
        : std::runtime_error("TaskRepository: task already exists: '" + id.value() + "'") {}
};

// ---------------------------------------------------------------------------
// ITaskRepository: contrato de persistência (Repository Pattern)
//
// - Intervalos de tempo são semiabertos: [from, to).
// - Intervalos inválidos (to < from) lançam std::invalid_argument.
// - Os resultados são devolvidos por valor (cópias), então o chamador nunca
//   mantém referências para o estado interno do repositório.
// ---------------------------------------------------------------------------
class ITaskRepository {
public:
    using Clock = Task::Clock;
    using TimePoint = Task::TimePoint;

    virtual ~ITaskRepository() = default;

    // --- Create / Update / Delete ------------------------------------------
    // Lança DuplicateTaskError se já existir uma tarefa com o mesmo id.
    virtual void add(const Task& task) = 0;

    // Substitui a tarefa persistida que possui o mesmo id.
    // Lança TaskNotFoundError se ela não existir.
    virtual void update(const Task& task) = 0;

    // Remove a tarefa. Retorna true se ela existia.
    virtual bool remove(const TaskId& id) = 0;

    // --- Read --------------------------------------------------------------
    virtual std::optional<Task> findById(const TaskId& id) const = 0;
    virtual bool existsById(const TaskId& id) const = 0;
    virtual std::vector<Task> findAll() const = 0;  // ordenado por createdAt

    // --- Consultas otimizadas ----------------------------------------------
    // Ordenadas por data de criação (crescente).
    virtual std::vector<Task> findByStatus(Status status) const = 0;
    virtual std::vector<Task> findByPriority(Priority priority) const = 0;

    // Ordenada por deadline (crescente). Tarefas sem deadline não aparecem.
    virtual std::vector<Task> findByDeadlineBetween(TimePoint from, TimePoint to) const = 0;

    // Tarefas vencidas em "now", segundo Task::isOverdue. Ordenada por deadline.
    virtual std::vector<Task> findOverdue(TimePoint now = Clock::now()) const = 0;

    // Ordenada por data de criação (crescente).
    virtual std::vector<Task> findByCreatedBetween(TimePoint from, TimePoint to) const = 0;

    // --- Contagens ---------------------------------------------------------
    virtual std::size_t count() const = 0;
    virtual std::size_t countByStatus(Status status) const = 0;
};

// ---------------------------------------------------------------------------
// InMemoryTaskRepository: implementação em memória com índices secundários.
//
// Estrutura de dados:
//   - tasks_          : unordered_map id -> Task            (acesso por id O(1))
//   - createdIndex_   : set ordenado por createdAt          (faixa de criação)
//   - deadlineIndex_  : set ordenado por deadline           (faixa / vencidas)
//   - byStatus_[s]    : um set por Status, ordenado por createdAt
//   - byPriority_[p]  : um set por Priority, ordenado por createdAt
//
// Complexidades (n = total de tarefas, k = tamanho do resultado):
//   add / update / remove ........ O(log n)
//   findById / existsById ........ O(1) médio
//   findByStatus / ByPriority .... O(k)   (sem varrer o repositório)
//   findByDeadlineBetween ........ O(log n + k)
//   findByCreatedBetween ......... O(log n + k)
//   findOverdue .................. O(log n + d), d = tarefas com deadline <= now
//   countByStatus / count ........ O(1)
//
// Thread-safety: leituras concorrentes (shared_mutex); escritas exclusivas.
// A cópia armazenada é privada ao repositório, por isso os índices sempre
// refletem exatamente os campos da cópia persistida.
// ---------------------------------------------------------------------------
class InMemoryTaskRepository final : public ITaskRepository {
public:
    InMemoryTaskRepository() = default;
    InMemoryTaskRepository(const InMemoryTaskRepository&) = delete;
    InMemoryTaskRepository& operator=(const InMemoryTaskRepository&) = delete;
    InMemoryTaskRepository(InMemoryTaskRepository&&) = delete;
    InMemoryTaskRepository& operator=(InMemoryTaskRepository&&) = delete;

    // --- Create / Update / Delete ------------------------------------------
    void add(const Task& task) override {
        std::unique_lock lock(mutex_);

        const auto [it, inserted] = tasks_.try_emplace(task.id().value(), task);
        if (!inserted) {
            throw DuplicateTaskError(task.id());
        }
        addToIndexes(it->second);
    }

    void update(const Task& task) override {
        std::unique_lock lock(mutex_);

        const auto it = tasks_.find(task.id().value());
        if (it == tasks_.end()) {
            throw TaskNotFoundError(task.id());
        }
        // Os índices dependem dos campos da cópia armazenada: remove as chaves
        // antigas antes de sobrescrever e reinsere com as novas.
        removeFromIndexes(it->second);
        it->second = task;
        addToIndexes(it->second);
    }

    bool remove(const TaskId& id) override {
        std::unique_lock lock(mutex_);

        const auto it = tasks_.find(id.value());
        if (it == tasks_.end()) {
            return false;
        }
        removeFromIndexes(it->second);
        tasks_.erase(it);
        return true;
    }

    // --- Read --------------------------------------------------------------
    std::optional<Task> findById(const TaskId& id) const override {
        std::shared_lock lock(mutex_);

        const auto it = tasks_.find(id.value());
        if (it == tasks_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool existsById(const TaskId& id) const override {
        std::shared_lock lock(mutex_);
        return tasks_.find(id.value()) != tasks_.end();
    }

    std::vector<Task> findAll() const override {
        std::shared_lock lock(mutex_);
        return collectAll(createdIndex_);
    }

    // --- Consultas otimizadas ----------------------------------------------
    std::vector<Task> findByStatus(Status status) const override {
        std::shared_lock lock(mutex_);
        return collectAll(byStatus_[slot(status)]);
    }

    std::vector<Task> findByPriority(Priority priority) const override {
        std::shared_lock lock(mutex_);
        return collectAll(byPriority_[slot(priority)]);
    }

    std::vector<Task> findByDeadlineBetween(TimePoint from, TimePoint to) const override {
        requireValidRange(from, to, "findByDeadlineBetween");
        std::shared_lock lock(mutex_);
        return collect(deadlineIndex_.lower_bound(from), deadlineIndex_.lower_bound(to), acceptAll);
    }

    std::vector<Task> findOverdue(TimePoint now = Clock::now()) const override {
        std::shared_lock lock(mutex_);
        // O índice limita a varredura às tarefas com deadline <= now. O filtro
        // final delega a regra de "vencida" à entidade (ex.: ignora concluídas).
        return collect(deadlineIndex_.begin(), deadlineIndex_.upper_bound(now),
                       [now](const Task& task) { return task.isOverdue(now); });
    }

    std::vector<Task> findByCreatedBetween(TimePoint from, TimePoint to) const override {
        requireValidRange(from, to, "findByCreatedBetween");
        std::shared_lock lock(mutex_);
        return collect(createdIndex_.lower_bound(from), createdIndex_.lower_bound(to), acceptAll);
    }

    // --- Contagens ---------------------------------------------------------
    std::size_t count() const override {
        std::shared_lock lock(mutex_);
        return tasks_.size();
    }

    std::size_t countByStatus(Status status) const override {
        std::shared_lock lock(mutex_);
        return byStatus_[slot(status)].size();
    }

private:
    // Chave de índice: (instante, id). O id desempata instantes iguais, o que
    // permite várias tarefas com o mesmo deadline/createdAt no mesmo set.
    struct IndexKey {
        TimePoint time;
        TaskId id;
    };

    // Comparador transparente: permite lower_bound/upper_bound direto por
    // TimePoint, sem fabricar um TaskId "mínimo" ou "máximo".
    struct IndexKeyLess {
        using is_transparent = void;

        bool operator()(const IndexKey& lhs, const IndexKey& rhs) const noexcept {
            if (lhs.time != rhs.time) {
                return lhs.time < rhs.time;
            }
            return lhs.id < rhs.id;
        }
        bool operator()(const IndexKey& lhs, TimePoint rhs) const noexcept { return lhs.time < rhs; }
        bool operator()(TimePoint lhs, const IndexKey& rhs) const noexcept { return lhs < rhs.time; }
    };

    using TimeIndex = std::set<IndexKey, IndexKeyLess>;

    // O último enumerador de cada enum define o tamanho dos arrays de índices.
    // Ao adicionar um novo valor em Status/Priority, atualize estas constantes.
    static constexpr std::size_t kStatusCount = static_cast<std::size_t>(Status::Completed) + 1;
    static constexpr std::size_t kPriorityCount = static_cast<std::size_t>(Priority::Critical) + 1;

    static std::size_t slot(Status status) noexcept { return static_cast<std::size_t>(status); }
    static std::size_t slot(Priority priority) noexcept { return static_cast<std::size_t>(priority); }

    static bool acceptAll(const Task&) noexcept { return true; }

    static void requireValidRange(TimePoint from, TimePoint to, const char* operation) {
        if (to < from) {
            throw std::invalid_argument(std::string("TaskRepository::") + operation +
                                        ": invalid range, 'to' precedes 'from'");
        }
    }

    void addToIndexes(const Task& task) {
        const IndexKey createdKey{task.createdAt(), task.id()};
        createdIndex_.insert(createdKey);
        byStatus_[slot(task.status())].insert(createdKey);
        byPriority_[slot(task.priority())].insert(createdKey);
        if (task.deadline().has_value()) {
            deadlineIndex_.insert(IndexKey{*task.deadline(), task.id()});
        }
    }

    void removeFromIndexes(const Task& task) {
        const IndexKey createdKey{task.createdAt(), task.id()};
        createdIndex_.erase(createdKey);
        byStatus_[slot(task.status())].erase(createdKey);
        byPriority_[slot(task.priority())].erase(createdKey);
        if (task.deadline().has_value()) {
            deadlineIndex_.erase(IndexKey{*task.deadline(), task.id()});
        }
    }

    // Materializa as tarefas apontadas pelas chaves de [first, last) que
    // satisfaçam o predicado. Exige que o mutex já esteja adquirido.
    template <typename Iterator, typename Predicate>
    std::vector<Task> collect(Iterator first, Iterator last, Predicate keep) const {
        std::vector<Task> result;
        for (; first != last; ++first) {
            const Task& task = tasks_.at(first->id.value());
            if (keep(task)) {
                result.push_back(task);
            }
        }
        return result;
    }

    std::vector<Task> collectAll(const TimeIndex& index) const {
        std::vector<Task> result;
        result.reserve(index.size());
        for (const IndexKey& key : index) {
            result.push_back(tasks_.at(key.id.value()));
        }
        return result;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Task> tasks_;
    TimeIndex createdIndex_;
    TimeIndex deadlineIndex_;
    std::array<TimeIndex, kStatusCount> byStatus_;
    std::array<TimeIndex, kPriorityCount> byPriority_;
};

}  // namespace ITaskRepository
#endif  // TASK_REPOSITORY
