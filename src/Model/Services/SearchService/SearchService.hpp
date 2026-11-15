#ifndef SEARCH_SERVICE_HPP
#define SEARCH_SERVICE_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "src/Model/Repositories/ITaskRepository.hpp"
#include "src/Model/Structures/TRIE/TRIE.hpp"

namespace SearchService {

enum class SortField { Title, Priority, Status, Deadline };
enum class SortOrder { Ascending, Descending };

struct SortSpec {
    SortField field = SortField::Title;
    SortOrder order = SortOrder::Ascending;
};

class SearchService {
public:
    using Repository = ITaskRepository::ITaskRepository;
    using TimePoint = Repository::TimePoint;

    explicit SearchService(const Repository& repository);

    std::optional<Task> searchById(const TaskId& id) const;
    std::vector<Task> searchByTitle(const std::string& term) const;
    std::vector<Task> searchByDescription(const std::string& term) const;
    std::vector<Task> searchByStatus(Status status) const;
    std::vector<Task> searchByPriority(Priority priority) const;

    // Intervalo fechado [from, to], como antes da refatoração.
    std::vector<Task> searchByDeadline(TimePoint from, TimePoint to) const;

    // Intervalo fechado de prioridades [from, to]. Por padrão, ordena por
    // prioridade crescente.
    std::vector<Task> searchByPriorityRange(
        Priority from, Priority to,
        SortSpec spec = {SortField::Priority, SortOrder::Ascending}) const;

    // Intervalo lexicográfico fechado de títulos [from, to], sem distinguir
    // maiúsculas de minúsculas. Por padrão, ordena por título crescente.
    std::vector<Task> searchByTitleRange(
        const std::string& from, const std::string& to,
        SortSpec spec = {SortField::Title, SortOrder::Ascending}) const;

    // Todas as tarefas, ordenadas segundo 'spec'.
    std::vector<Task> searchAll(SortSpec spec = {}) const;

    // Ordena (de forma estável) qualquer resultado de busca. Em Deadline,
    // tarefas sem prazo ficam sempre no final, em ambas as ordens.
    static void sortTasks(std::vector<Task>& tasks, SortSpec spec);

    std::vector<std::string> autocomplete(const std::string& prefix,
                                          std::size_t maxResults = 0) const;

    void indexTask(const Task& task);
    void removeTaskFromIndex(const TaskId& id);
    void rebuildIndex();

private:
    const Repository& repository_;
    Trie index_;
};

}  // namespace SearchService
#endif // SEARCH_SERVICE_HPP