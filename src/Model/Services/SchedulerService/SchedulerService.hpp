#ifndef SCHEDULER_SERVICE
#define SCHEDULER_SERVICE

#include <chrono>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "src/Model/Entities/Task/Task.hpp"

namespace SchedulerService {

// ---------------------------------------------------------------------------
// ScheduledTask: visão de uma tarefa dentro do agendamento.
// Guarda um snapshot da Task e a prioridade "de agendamento". Essa prioridade
// começa igual à da Task, mas pode divergir após updateTaskPriority, que NÃO
// altera a entidade Task.
// ---------------------------------------------------------------------------
struct ScheduledTask {
    Task task;
    Priority priority;
};

// ---------------------------------------------------------------------------
// ScheduledTaskOrder: critério de ordenação do scheduler.
//   1. Maior prioridade (de agendamento) primeiro;
//   2. Menor prazo primeiro (tarefas sem prazo vêm depois das que têm);
//   3. Menor id (ordem lexicográfica do TaskId) como desempate.
// ---------------------------------------------------------------------------
struct ScheduledTaskOrder {
    bool operator()(const ScheduledTask& lhs, const ScheduledTask& rhs) const noexcept;
};

// ---------------------------------------------------------------------------
// SchedulerService:  Serviço que mantém as tarefas ordenadas.
//
// - Trabalha sobre cópias (snapshots) das tarefas; para refletir mudanças
//   feitas nas Tasks originais, use refreshScheduler.
// - Tarefas concluídas nunca entram no agendamento.
// - Não é thread-safe; sincronize externamente se necessário.
// - Não é copiável (o índice interno guarda iteradores); é movível.
// ---------------------------------------------------------------------------
class SchedulerService {
public:
    using Clock = Task::Clock;
    using TimePoint = Task::TimePoint;
    using Duration = Clock::duration;

    static constexpr std::chrono::hours kDefaultUrgencyWindow{24};

    SchedulerService() = default;
    SchedulerService(const SchedulerService&) = delete;
    SchedulerService& operator=(const SchedulerService&) = delete;
    SchedulerService(SchedulerService&&) noexcept = default;
    SchedulerService& operator=(SchedulerService&&) noexcept = default;
    ~SchedulerService() = default;

    // Monta o agendamento inicial. Exige scheduler vazio (senão, std::logic_error)
    // e ids únicos (senão, std::invalid_argument). Ignora tarefas concluídas.
    void buildSchedule(const std::vector<Task>& tasks);

    // Descarta todo o agendamento atual (inclusive prioridades alteradas via
    // updateTaskPriority) e o reconstrói a partir de 'tasks'.
    void rebuildSchedule(const std::vector<Task>& tasks);

    // Sincroniza o agendamento com o estado atual das tarefas: remove as que
    // sumiram ou foram concluídas, inclui as novas e atualiza os snapshots,
    // PRESERVANDO as prioridades alteradas via updateTaskPriority.
    void refreshScheduler(const std::vector<Task>& currentTasks);

    // Próxima tarefa disponível (pendente) na ordem de agendamento.
    std::optional<ScheduledTask> getNextTask() const;

    // Tarefas cuja prioridade de agendamento é 'priority', em ordem de agendamento.
    std::vector<ScheduledTask> getTaskByPriority(Priority priority) const;

    // Tarefas com prazo definido e menor ou igual a 'limit'.
    std::vector<ScheduledTask> getTaskByDeadline(TimePoint limit) const;

    // Tarefas urgentes: prioridade de agendamento Critical OU prazo menor ou
    // igual a now + urgencyWindow (inclui as já atrasadas).
    std::vector<ScheduledTask> getUrgentTasks(TimePoint now = Clock::now(),
                                              Duration urgencyWindow = kDefaultUrgencyWindow) const;

    // Tarefas atrasadas em 'now' (conforme Task::isOverdue).
    std::vector<ScheduledTask> getOverdueTasks(TimePoint now = Clock::now()) const;

    // Tarefas disponíveis para iniciar (pendentes), em ordem de agendamento.
    std::vector<ScheduledTask> getAvaliableTasks() const;

    // Altera a prioridade SOMENTE na estrutura de agendamento (a Task não muda)
    // e reposiciona a tarefa. Retorna false se o id não está agendado.
    [[nodiscard]] bool updateTaskPriority(const TaskId& id, Priority newPriority);

    // Remove a tarefa do agendamento. Retorna false se o id não está agendado.
    [[nodiscard]] bool removeTaskFromScheduler(const TaskId& id);

    std::size_t size() const noexcept { return storage_.ordered.size(); }
    bool empty() const noexcept { return storage_.ordered.empty(); }

private:
    using OrderedTasks = std::set<ScheduledTask, ScheduledTaskOrder>;

    // 'ordered' mantém a ordem; 'index' dá acesso O(log n) por id.
    // Mover/trocar std::set preserva a validade dos iteradores.
    struct Storage {
        OrderedTasks ordered;
        std::map<TaskId, OrderedTasks::iterator> index;
    };

    static void insertEntry(Storage& storage, ScheduledTask entry);

    // Constrói um Storage novo; se 'previous' for informado, preserva as
    // prioridades de agendamento das tarefas já conhecidas.
    static Storage buildStorage(const std::vector<Task>& tasks, const Storage* previous);

    template <typename Predicate>
    std::vector<ScheduledTask> collect(Predicate predicate) const;

    Storage storage_;
};

}  // namespace SchedulerService

#endif // SCHEDULER_SERVICE
