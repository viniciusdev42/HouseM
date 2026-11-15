#ifndef SEARCH_SERVICE
#define SEARCH_SERVICE

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "src/Model/Repositories/ITaskRepository.hpp"
#include "src/Model/Structures/TRIE/TRIE.hpp"

namespace SearchService {

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

    // Palavras (minúsculas, ordem lexicográfica) de títulos e descrições que
    // começam com o prefixo. maxResults == 0 significa sem limite.
    std::vector<std::string> autocomplete(const std::string& prefix,
                                          std::size_t maxResults = 0) const;

    // Manutenção do índice: o repositório não notifica mudanças, então quem
    // cria, edita ou exclui tarefas deve chamar estes métodos.
    void indexTask(const Task& task);
    void removeTaskFromIndex(const TaskId& id);
    void rebuildIndex();

private:
    const Repository& repository_;
    Trie index_;
};

}  // namespace SearchService
#endif // SEARCH_SERVICE
