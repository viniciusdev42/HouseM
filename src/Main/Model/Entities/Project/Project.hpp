#ifndef PROJECT_HPP
#define PROJECT_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "src/Main/Model/Entities/Task/Task.hpp"

namespace Project {

using ProjectId = std::uint64_t;
using TaskId = Task::TaskId;
using Clock = Task::Task::Clock;
using TimePoint = Task::Task::TimePoint;

/*
  1.Active:em andamento; permite alterações de metadados, transições de estado e 
           adição/remoção de tarefas.

  2.Completed: concluído; aceita edição de metadados e reabertura/arquivamento, 
                 mas não aceita novas tarefas.

  3.Archived: arquivado; bloqueia edições, remoção de tarefas e transições diretas 
                         exceto unarchive().
*/
enum class ProjectStatus : std::uint8_t {
    Active, 
    Completed,
    Archived
};

/* Converte o enum em std::string_view ("Active", "Completed", "Archived" ou "Unknown")
   para logs, serialização ou UI.*/
std::string_view toString(ProjectStatus status) noexcept;

class Project {
public:
    static constexpr ProjectId kInvalidId = 0;
    static constexpr std::size_t kMaxNameLength = 120;
    static constexpr std::size_t kMaxDescriptionLength = 4096;
    /* 
      Cria um novo projeto sempre no estado Active, com lista de tarefas vazia e updatedAt_ igual a createdAt. 
      Valida id, normaliza/valida name (remove espaços nas extremidades, não pode ser vazio nem passar de kMaxNameLength), 
      valida description (kMaxDescriptionLength) e checa se deadline não é anterior a createdAt. 
      Lança std::invalid_argument se algo falhar.*/ 
    Project(ProjectId id,
            std::string name,
            std::string description = {},
            std::optional<TimePoint> deadline = std::nullopt,
            TimePoint createdAt = Clock::now());

    /* Reconstrói um projeto já salvo (ex.: vindo de repositório/serialização),
       preservando status, updatedAt e taskIds.
       Valida updatedAt >= createdAt, 
       checa a lista de taskIds (sem duplicatas ou IDs inválidos) 
       e aplica os invariantes básicos do projeto.*/
    static Project restore(ProjectId id,
                           std::string name,
                           std::string description,
                           ProjectStatus status,
                           std::optional<TimePoint> deadline,
                           TimePoint createdAt,
                           TimePoint updatedAt,
                           std::vector<TaskId> taskIds);

    // --- Consultas ---

    ProjectId id() const noexcept { return id_; }
    const std::string& name() const noexcept { return name_; }
    const std::string& description() const noexcept { return description_; }
    ProjectStatus status() const noexcept { return status_; }
    const std::optional<TimePoint>& deadline() const noexcept { return deadline_; }
    TimePoint createdAt() const noexcept { return createdAt_; }
    TimePoint updatedAt() const noexcept { return updatedAt_; }

    const std::vector<TaskId>& taskIds() const noexcept { return taskIds_; }
    std::size_t taskCount() const noexcept { return taskIds_.size(); }
    /* Verifica se taskId está presente em taskIds_ */
    bool hasTask(const TaskId& taskId) const noexcept;

    bool isActive() const noexcept { return status_ == ProjectStatus::Active; }
    bool isCompleted() const noexcept { return status_ == ProjectStatus::Completed; }
    bool isArchived() const noexcept { return status_ == ProjectStatus::Archived; }

    /* Retorna true apenas se o projeto estiver Active, possuir deadline_ e now for posterior ao prazo */
    bool isOverdue(TimePoint now = Clock::now()) const noexcept;

    // --- Edição de metadados (exige projeto não arquivado) ---

    void rename(std::string newName, TimePoint now = Clock::now());
    void setDescription(std::string newDescription, TimePoint now = Clock::now());
    void setDeadline(TimePoint deadline, TimePoint now = Clock::now());
    void clearDeadline(TimePoint now = Clock::now());

    // --- Tarefas ---

    /* Exige status Active (senão lança std::logic_error), 
       valida taskId (std::invalid_argument), 
       retorna false se já estiver associada e, caso contrário, adiciona ao final de taskIds_, 
       atualiza updatedAt_ e retorna true.*/
    bool addTask(TaskId taskId, TimePoint now = Clock::now());

    /* Exige projeto não arquivado (std::logic_error); 
       retorna false se o ID não existir na lista ou remove o elemento, 
       atualiza updatedAt_ e retorna true.*/
    bool removeTask(const TaskId& taskId, TimePoint now = Clock::now());

    // --- Transições de estado ---

    void complete(TimePoint now = Clock::now());
    void reopen(TimePoint now = Clock::now());
    void archive(TimePoint now = Clock::now());
    void unarchive(TimePoint now = Clock::now());

    friend bool operator==(const Project& a, const Project& b) noexcept {
        return a.id_ == b.id_;
    }
    friend bool operator!=(const Project& a, const Project& b) noexcept {
        return !(a == b);
    }

private:
    void ensureNotArchived(std::string_view operation) const;
    void touch(TimePoint now) noexcept;

    ProjectId id_;
    std::string name_;
    std::string description_;
    ProjectStatus status_{ProjectStatus::Active};
    std::optional<TimePoint> deadline_;
    TimePoint createdAt_;
    TimePoint updatedAt_;
    std::vector<TaskId> taskIds_;
};

}  // namespace Project

#endif  // PROJECT_HPP