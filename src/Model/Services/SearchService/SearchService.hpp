#ifndef SEARCH_SERVICE
#define SEARCH_SERVICE

#include <optional>
#include <string>
#include <vector>

#include "src/Model/Repositories/ITaskRepository.hpp"

namespace SearchService {

class SearchService {
public:
    using Repository = ITaskRepository::ITaskRepository;
    using TimePoint = Repository::TimePoint;

    explicit SearchService(const Repository& repository) noexcept;

    std::optional<Task> searchById(const TaskId& id) const;
    std::vector<Task> searchByTitle(const std::string& term) const;
    std::vector<Task> searchByDescription(const std::string& term) const;
    std::vector<Task> searchByStatus(Status status) const;
    std::vector<Task> searchByPriority(Priority priority) const;

    // Intervalo fechado [from, to], como antes da refatoração.
    std::vector<Task> searchByDeadline(TimePoint from, TimePoint to) const;

private:
    const Repository& repository_;
};

}  // namespace SearchService
#endif // SEARCH_SERVICE