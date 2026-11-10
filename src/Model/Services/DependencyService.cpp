#include "DependencyService.hpp" 
 
 bool addDependency(const TaskId& prerequisiteId, const TaskId& dependentId) {
        if (!Dependency::isValidPair(prerequisiteId, dependentId)) {
            throw std::invalid_argument("DependencyService: invalid dependency pair " +
                                        prerequisiteId.value() + " -> " + dependentId.value());
        }

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
    
    bool removeDependency(const TaskId& prerequisiteId, const TaskId& dependentId) {
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
  
    std::size_t removeTaskDependencies(const TaskId& taskId) {
        const auto it = ids_.find(taskId);
        if (it == ids_.end()) {
            return 0;
        }

        const Vertex vertex = it->second;
        std::vector<Vertex> neighbors;  // vizinhos podem ficar isolados após a remoção
        const std::size_t removed = graph_.getDegree(vertex);

        for (const auto& [other, otherId] : tasks_) {
            if (other != vertex &&
                (graph_.hasEdge(vertex, other) || graph_.hasEdge(other, vertex))) {
                neighbors.push_back(other);
            }
        }

        eraseVertex(vertex);
        for (Vertex neighbor : neighbors) {
            eraseIfIsolated(neighbor);
        }
        return removed;
    }

    std::vector<Dependency> getDependencies(const TaskId& taskId) const {
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
    std::vector<Dependency> getDependents(const TaskId& taskId) const {
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
    
    bool hasDependency(const TaskId& prerequisiteId, const TaskId& dependentId) const {
        const auto prerequisite = ids_.find(prerequisiteId);
        const auto dependent = ids_.find(dependentId);
        if (prerequisite == ids_.end() || dependent == ids_.end()) {
            return false;
        }
        return graph_.hasEdge(prerequisite->second, dependent->second);
    }
    
    bool hasPath(const TaskId& from, const TaskId& to) const {
        const auto source = ids_.find(from);
        const auto target = ids_.find(to);
        if (source == ids_.end() || target == ids_.end()) {
            return false;
        }
        return hasPathBetween(source->second, target->second);
    }
    
    bool detectCycle() const {
        return graph_.hasCycle();
    }

    /// Indica se adicionar `prerequisiteId` -> `dependentId` criaria um ciclo,
    /// sem alterar o estado do serviço.
    bool detectCycle(const TaskId& prerequisiteId, const TaskId& dependentId) const {
        if (prerequisiteId == dependentId) {
            return true;
        }
        return hasPath(dependentId, prerequisiteId);
    }
    
    std::vector<TaskId> getBlockedTasks(const std::set<TaskId>& completedTasks) const {
        const auto pending = countPendingPrerequisites(completedTasks);

        std::vector<TaskId> result;
        for (const auto& [vertex, id] : tasks_) {
            if (completedTasks.count(id) == 0 && pending.at(vertex) > 0) {
                result.push_back(id);
            }
        }
        return result;
    }
    
    std::vector<TaskId> getAvailableTasks(const std::set<TaskId>& completedTasks) const;
        const auto pending = countPendingPrerequisites(completedTasks);

        std::vector<TaskId> result;
        for (const auto& [vertex, id] : tasks_) {
            if (completedTasks.count(id) == 0 && pending.at(vertex) == 0) {
                result.push_back(id);
            }
        }
        return result;
    
    std::vector<TaskId> getAvaliableTasks(const std::set<TaskId>& completedTasks) const {
        return getAvailableTasks(completedTasks);
    }

    std::vector<TaskId> getTopologicalOrder() const {
        const std::vector<Vertex> order = graph_.topologicalSort();

        std::vector<TaskId> result;
        result.reserve(order.size());
        for (Vertex vertex : order) {
            result.push_back(tasks_.at(vertex));
        }
        return result;
  }

private:
    using Vertex = Graph::Vertex;

.
    Vertex ensureVertex(const TaskId& id) {
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

    void eraseVertex(Vertex vertex) {
        graph_.removeVertex(vertex);
        ids_.erase(tasks_.at(vertex));
        tasks_.erase(vertex);
  }

    void eraseIfIsolated(Vertex vertex){
        if (tasks_.count(vertex) != 0 && graph_.getDegree(vertex) == 0) {
            eraseVertex(vertex);
        }
   }    


    bool hasPathBetween(Vertex from, Vertex to) const {
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

    std::unordered_map<Vertex, std::size_t>
    countPendingPrerequisites(const std::set<TaskId>& completedTasks) const {
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
