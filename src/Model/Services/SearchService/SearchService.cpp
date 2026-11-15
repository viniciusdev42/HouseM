#include "SearchService.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace SearchService {

namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

bool containsIgnoreCase(const std::string& text, const std::string& lowerTerm) {
    return toLower(text).find(lowerTerm) != std::string::npos;
}

void requireNonEmptyTerm(const std::string& term, const char* operation) {
    if (term.empty()) {
        throw std::invalid_argument(std::string("SearchService::") + operation +
                                    ": search term must not be empty");
    }
}

// Busca textual: o repositório não tem índice para texto, então varre findAll().
template <typename Predicate>
std::vector<Task> filterTasks(const SearchService::Repository& repository, Predicate predicate) {
    std::vector<Task> result;
    for (Task& task : repository.findAll()) {
        if (predicate(task)) {
            result.push_back(std::move(task));
        }
    }
    return result;
}

}  // namespace

SearchService::SearchService(const Repository& repository) noexcept
    : repository_(repository) {}

std::optional<Task> SearchService::searchById(const TaskId& id) const {
    return repository_.findById(id);
}

std::vector<Task> SearchService::searchByTitle(const std::string& term) const {
    requireNonEmptyTerm(term, "searchByTitle");
    const std::string lowerTerm = toLower(term);
    return filterTasks(repository_, [&lowerTerm](const Task& task) {
        return containsIgnoreCase(task.title(), lowerTerm);
    });
}

std::vector<Task> SearchService::searchByDescription(const std::string& term) const {
    requireNonEmptyTerm(term, "searchByDescription");
    const std::string lowerTerm = toLower(term);
    return filterTasks(repository_, [&lowerTerm](const Task& task) {
        return containsIgnoreCase(task.description(), lowerTerm);
    });
}

std::vector<Task> SearchService::searchByStatus(Status status) const {
    return repository_.findByStatus(status);
}

std::vector<Task> SearchService::searchByPriority(Priority priority) const {
    return repository_.findByPriority(priority);
}

std::vector<Task> SearchService::searchByDeadline(TimePoint from, TimePoint to) const {
    if (from > to) {
        throw std::invalid_argument(
            "SearchService::searchByDeadline: 'from' must not be later than 'to'");
    }

    // O repositório usa [from, to); o serviço expõe [from, to]. Para incluir
    // 'to', avança um tick no limite superior.
    if (to < TimePoint::max()) {
        return repository_.findByDeadlineBetween(from, to + TimePoint::duration{1});
    }

    // Caso extremo (to == TimePoint::max()): não há tick seguinte. Consulta
    // [from, max) e acrescenta as tarefas com deadline exatamente em max,
    // mantendo a ordenação por deadline.
    std::vector<Task> result = repository_.findByDeadlineBetween(from, to);
    for (Task& task : repository_.findAll()) {
        if (task.deadline().has_value() && *task.deadline() == to) {
            result.push_back(std::move(task));
        }
    }
    return result;
}

}  // namespace SearchService