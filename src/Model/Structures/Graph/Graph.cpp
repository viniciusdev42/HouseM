#include "Graph.hpp"

#include <algorithm>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

// Remove a primeira ocorrência de `value`. Retorna true se algo foi removido.
bool eraseOne(Graph::NeighborList& list, Graph::Vertex value) {
    auto it = std::find(list.begin(), list.end(), value);
    if (it == list.end()) {
        return false;
    }
    list.erase(it);
    return true;
}

enum class Color { White, Gray, Black };

} // namespace

Graph::Graph(bool directed) : directed_(directed), edgeCount_(0) {}

// ---- Vértices --------------------------------------------------------------

bool Graph::addVertex(Vertex v) {
    return adjacency_.try_emplace(v).second;
}

bool Graph::removeVertex(Vertex v) {
    auto it = adjacency_.find(v);
    if (it == adjacency_.end()) {
        return false;
    }

    // Dirigido: arestas de saída de v. Não dirigido: todas as arestas incidentes.
    edgeCount_ -= it->second.size();
    adjacency_.erase(it);

    for (auto& entry : adjacency_) {
        if (eraseOne(entry.second, v) && directed_) {
            --edgeCount_; // aresta de entrada (other -> v)
        }
    }
    return true;
}

bool Graph::hasVertex(Vertex v) const {
    return adjacency_.find(v) != adjacency_.end();
}

// ---- Arestas ---------------------------------------------------------------

bool Graph::addEdge(Vertex from, Vertex to) {
    if (!hasVertex(from)) {
        throw std::invalid_argument("addEdge: vertice inexistente: " + std::to_string(from));
    }
    if (!hasVertex(to)) {
        throw std::invalid_argument("addEdge: vertice inexistente: " + std::to_string(to));
    }
    if (!directed_ && from == to) {
        throw std::invalid_argument("addEdge: lacos nao sao permitidos em grafos nao dirigidos");
    }
    if (hasEdge(from, to)) {
        return false;
    }

    adjacency_[from].push_back(to);
    if (!directed_) {
        adjacency_[to].push_back(from);
    }
    ++edgeCount_;
    return true;
}

bool Graph::removeEdge(Vertex from, Vertex to) {
    if (!hasEdge(from, to)) {
        return false;
    }

    eraseOne(adjacency_[from], to);
    if (!directed_) {
        eraseOne(adjacency_[to], from);
    }
    --edgeCount_;
    return true;
}

bool Graph::hasEdge(Vertex from, Vertex to) const {
    auto it = adjacency_.find(from);
    if (it == adjacency_.end() || !hasVertex(to)) {
        return false;
    }
    const NeighborList& list = it->second;
    return std::find(list.begin(), list.end(), to) != list.end();
}

// ---- Consultas -------------------------------------------------------------

const Graph::NeighborList& Graph::requireVertex(Vertex v) const {
    auto it = adjacency_.find(v);
    if (it == adjacency_.end()) {
        throw std::invalid_argument("vertice inexistente: " + std::to_string(v));
    }
    return it->second;
}

const Graph::NeighborList& Graph::getNeighbors(Vertex v) const {
    return requireVertex(v);
}

std::size_t Graph::getDegree(Vertex v) const {
    const NeighborList& out = requireVertex(v);
    if (!directed_) {
        return out.size();
    }

    std::size_t in = 0;
    for (const auto& entry : adjacency_) {
        in += static_cast<std::size_t>(
            std::count(entry.second.begin(), entry.second.end(), v));
    }
    return in + out.size();
}

std::size_t Graph::vertexCount() const noexcept {
    return adjacency_.size();
}

std::size_t Graph::edgeCount() const noexcept {
    return edgeCount_;
}

bool Graph::isDirected() const noexcept {
    return directed_;
}

void Graph::clear() noexcept {
    adjacency_.clear();
    edgeCount_ = 0;
}

void Graph::print(std::ostream& os) const {
    for (const auto& entry : adjacency_) {
        os << entry.first << " ->";
        for (Vertex neighbor : entry.second) {
            os << ' ' << neighbor;
        }
        os << '\n';
    }
}

// ---- Detecção de ciclos ----------------------------------------------------

bool Graph::hasCycle() const {
    return directed_ ? hasCycleDirected() : hasCycleUndirected();
}

// DFS iterativa (evita estouro de pilha em grafos profundos).
// Uma aresta para um vértice "cinza" (em exploração) é uma aresta de retorno => ciclo.
bool Graph::hasCycleDirected() const {
    std::map<Vertex, Color> color;
    for (const auto& entry : adjacency_) {
        color[entry.first] = Color::White;
    }

    for (const auto& entry : adjacency_) {
        if (color[entry.first] != Color::White) {
            continue;
        }

        std::vector<std::pair<Vertex, std::size_t>> stack;
        stack.emplace_back(entry.first, 0);
        color[entry.first] = Color::Gray;

        while (!stack.empty()) {
            const Vertex current = stack.back().first;
            const NeighborList& neighbors = adjacency_.at(current);
            std::size_t& nextIndex = stack.back().second;

            if (nextIndex == neighbors.size()) {
                color[current] = Color::Black;
                stack.pop_back();
                continue;
            }

            const Vertex next = neighbors[nextIndex++];
            if (color[next] == Color::Gray) {
                return true;
            }
            if (color[next] == Color::White) {
                color[next] = Color::Gray;
                stack.emplace_back(next, 0); // invalida nextIndex; não usar depois
            }
        }
    }
    return false;
}

// Sem arestas paralelas nem laços, uma floresta com C componentes tem V - C arestas.
// Qualquer aresta além disso implica a existência de um ciclo.
bool Graph::hasCycleUndirected() const {
    std::set<Vertex> visited;
    std::size_t components = 0;

    for (const auto& entry : adjacency_) {
        if (visited.count(entry.first) != 0) {
            continue;
        }
        ++components;

        std::queue<Vertex> pending;
        pending.push(entry.first);
        visited.insert(entry.first);

        while (!pending.empty()) {
            const Vertex current = pending.front();
            pending.pop();
            for (Vertex neighbor : adjacency_.at(current)) {
                if (visited.insert(neighbor).second) {
                    pending.push(neighbor);
                }
            }
        }
    }
    return edgeCount_ > adjacency_.size() - components;
}

// ---- Ordenação topológica --------------------------------------------------

std::vector<Graph::Vertex> Graph::topologicalSort() const {
    if (!directed_) {
        throw std::logic_error("topologicalSort: requer um grafo dirigido");
    }

    std::map<Vertex, std::size_t> inDegree;
    for (const auto& entry : adjacency_) {
        inDegree.try_emplace(entry.first, 0);
    }
    for (const auto& entry : adjacency_) {
        for (Vertex neighbor : entry.second) {
            ++inDegree[neighbor];
        }
    }

    std::queue<Vertex> ready;
    for (const auto& entry : inDegree) {
        if (entry.second == 0) {
            ready.push(entry.first);
        }
    }

    std::vector<Vertex> order;
    order.reserve(adjacency_.size());

    while (!ready.empty()) {
        const Vertex current = ready.front();
        ready.pop();
        order.push_back(current);

        for (Vertex neighbor : adjacency_.at(current)) {
            if (--inDegree[neighbor] == 0) {
                ready.push(neighbor);
            }
        }
    }

    // Vértices em ciclos nunca chegam a grau de entrada 0.
    if (order.size() != adjacency_.size()) {
        throw std::logic_error("topologicalSort: o grafo contem ciclo");
    }
    return order;
}
