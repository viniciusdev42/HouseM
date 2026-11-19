#include "DependencyService.hpp"

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace DependencyService {

DependencyService::DependencyService(const TaskRepository& repository)
    : repository_(repository), graph_(true) {}

// --- Escrita ---------------------------------------------------------------

bool DependencyService::addDependency(const TaskId& prerequisiteId, const TaskId& dependentId) {
    if (!Dependency::isValidPair(prerequisiteId, dependentId)) {
        throw std::invalid_argument("DependencyService: invalid dependency pair " +
                                    prerequisiteId.value() + " -> " + dependentId.value());
    }

    requireTaskExists(prerequisiteId);
    requireTaskExists(dependentId);

    const auto prerequisite = ids_.find(prerequisiteId);
    const auto dependent = ids_.find(dependentId);

    // Se alguma das tarefas ainda não é rastreada, ela não tem arestas e
    // portanto não há como fechar um ciclo.
    if (prerequisite != ids_.end() && dependent != ids_.end()) {
        if (graph_.hasEdge(prerequisite->second, dependent->second)) {
            return false;
        }
        if (hasPathBetween(dependent->second, prerequisite->second)) {
            throw std::logic_error("DependencyService: dependency " + prerequisiteId.value() +
                                   " -> " + dependentId.value() + " would create a cycle");
        }
    }

    const Vertex from = ensureVertex(prerequisiteId);
    const Vertex to = ensureVertex(dependentId);
    graph_.addEdge(from, to);
    return true;
}

bool DependencyService::removeDependency(const TaskId& prerequisiteId, const TaskId& dependentId) {
    const auto prerequisite = ids_.find(prerequisiteId);
    const auto dependent = ids_.find(dependentId);
    if (prerequisite == ids_.end() || dependent == ids_.end()) {
        return false;
    }

    const Vertex from = prerequisite->second;
    const Vertex to = dependent->second;
    if (!graph_.removeEdge(from, to)) {
        return false;
    }

    eraseIfIsolated(from);
    eraseIfIsolated(to);
    return true;
}

std::size_t DependencyService::removeTaskDependencies(const TaskId& taskId) {
    const auto it = ids_.find(taskId);
    if (it == ids_.end()) {
        return 0;
    }

    const Vertex vertex = it->second;
    std::vector<Vertex> neighbors;  // vizinhos podem ficar isolados após a remoção

    for (const auto& entry : tasks_) {
        const Vertex other = entry.first;
        if (other != vertex &&
            (graph_.hasEdge(vertex, other) || graph_.hasEdge(other, vertex))) {
            neighbors.push_back(other);
        }
    }

    // Grafo acíclico: um vizinho nunca está ligado nos dois sentidos, logo
    // cada vizinho corresponde a exatamente uma dependência removida.
    const std::size_t removed = neighbors.size();

    eraseVertex(vertex);
    for (Vertex neighbor : neighbors) {
        eraseIfIsolated(neighbor);
    }
    return removed;
}

// --- Consultas -------------------------------------------------------------

std::vector<Dependency> DependencyService::getDependencies(const TaskId& taskId) const {
    std::vector<Dependency> result;
    const auto it = ids_.find(taskId);
    if (it == ids_.end()) {
        return result;
    }

    for (const auto& [vertex, id] : tasks_) {
        if (graph_.hasEdge(vertex, it->second)) {
            result.emplace_back(id, taskId);
        }
    }
    return result;
}

std::vector<Dependency> DependencyService::getDependents(const TaskId& taskId) const {
    std::vector<Dependency> result;
    const auto it = ids_.find(taskId);
    if (it == ids_.end()) {
        return result;
    }

    for (Vertex neighbor : graph_.getNeighbors(it->second)) {
        result.emplace_back(taskId, tasks_.at(neighbor));
    }
    return result;
}

bool DependencyService::hasDependency(const TaskId& prerequisiteId,
                                      const TaskId& dependentId) const {
    const auto prerequisite = ids_.find(prerequisiteId);
    const auto dependent = ids_.find(dependentId);
    if (prerequisite == ids_.end() || dependent == ids_.end()) {
        return false;
    }
    return graph_.hasEdge(prerequisite->second, dependent->second);
}

bool DependencyService::hasPath(const TaskId& from, const TaskId& to) const {
    const auto source = ids_.find(from);
    const auto target = ids_.find(to);
    if (source == ids_.end() || target == ids_.end()) {
        return false;
    }
    return hasPathBetween(source->second, target->second);
}

bool DependencyService::detectCycle() const {
    return graph_.hasCycle();
}

bool DependencyService::detectCycle(const TaskId& prerequisiteId,
                                    const TaskId& dependentId) const {
    if (prerequisiteId == dependentId) {
        return true;
    }
    return hasPath(dependentId, prerequisiteId);
}

std::vector<Task> DependencyService::getBlockedTasks(
    const std::set<TaskId>& completedTasks) const {
    return collectUncompleted(completedTasks, [](std::size_t pending) { return pending > 0; });
}

std::vector<Task> DependencyService::getAvailableTasks(
    const std::set<TaskId>& completedTasks) const {
    return collectUncompleted(completedTasks, [](std::size_t pending) { return pending == 0; });
}

std::vector<Task> DependencyService::getAvaliableTasks(
    const std::set<TaskId>& completedTasks) const {
    return getAvailableTasks(completedTasks);
}

std::vector<Task> DependencyService::getTopologicalOrder() const {
    const std::vector<Vertex> order = graph_.topologicalSort();

    std::vector<Task> result;
    result.reserve(order.size());
    for (Vertex vertex : order) {
        result.push_back(loadTask(tasks_.at(vertex)));
    }
    return result;
}

// --- Auxiliares privados ---------------------------------------------------

void DependencyService::requireTaskExists(const TaskId& id) const {
    if (!repository_.existsById(id)) {
        throw ::ITaskRepository::TaskNotFoundError(id);
    }
}

Task DependencyService::loadTask(const TaskId& id) const {
    auto task = repository_.findById(id);
    if (!task.has_value()) {
        throw ::ITaskRepository::TaskNotFoundError(id);
    }
    return std::move(*task);
}

DependencyService::Vertex DependencyService::ensureVertex(const TaskId& id) {
    const auto it = ids_.find(id);
    if (it != ids_.end()) {
        return it->second;
    }

    const Vertex vertex = nextVertex_++;
    graph_.addVertex(vertex);
    ids_.emplace(id, vertex);
    tasks_.emplace(vertex, id);
    return vertex;
}

void DependencyService::eraseVertex(Vertex vertex) {
    graph_.removeVertex(vertex);
    ids_.erase(tasks_.at(vertex));
    tasks_.erase(vertex);
}

void DependencyService::eraseIfIsolated(Vertex vertex) {
    if (tasks_.count(vertex) != 0 && graph_.getDegree(vertex) == 0) {
        eraseVertex(vertex);
    }
}

bool DependencyService::hasPathBetween(Vertex from, Vertex to) const {
    if (from == to) {
        return true;
    }

    std::unordered_set<Vertex> visited{from};
    std::vector<Vertex> stack{from};

    while (!stack.empty()) {
        const Vertex current = stack.back();
        stack.pop_back();

        for (Vertex neighbor : graph_.getNeighbors(current)) {
            if (neighbor == to) {
                return true;
            }
            if (visited.insert(neighbor).second) {
                stack.push_back(neighbor);
            }
        }
    }
    return false;
}

std::unordered_map<DependencyService::Vertex, std::size_t>
DependencyService::countPendingPrerequisites(const std::set<TaskId>& completedTasks) const {
    std::unordered_map<Vertex, std::size_t> pending;
    for (const auto& entry : tasks_) {
        pending.emplace(entry.first, 0);
    }

    for (const auto& [vertex, id] : tasks_) {
        if (completedTasks.count(id) != 0) {
            continue;
        }
        for (Vertex dependent : graph_.getNeighbors(vertex)) {
            ++pending[dependent];
        }
    }
    return pending;
}

template <typename Predicate>
std::vector<Task> DependencyService::collectUncompleted(const std::set<TaskId>& completedTasks,
                                                        Predicate keep) const {
    const auto pending = countPendingPrerequisites(completedTasks);

    std::vector<Task> result;
    for (const auto& [vertex, id] : tasks_) {
        if (completedTasks.count(id) == 0 && keep(pending.at(vertex))) {
            result.push_back(loadTask(id));
        }
    }
    return result;
}

}  // namespace DependencyService
