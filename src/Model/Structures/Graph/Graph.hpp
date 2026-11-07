#ifndef GRAPH
#define GRAPH

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Graph {

enum class GraphType { Directed, Undirected };

/**
 * Grafo representado por lista de adjacência (header-only, C++17).
 *
 * - Vértices são identificados por índices de 0 a vertexCount() - 1.
 * - Suporta grafos direcionados e não direcionados.
 * - Arestas paralelas e laços (self-loops) são permitidos; ambos contam
 *   como ciclo na detecção de ciclos.
 * - Todos os algoritmos são iterativos, evitando estouro de pilha em
 *   grafos profundos.
 *
 * Complexidade (V = vértices, E = arestas):
 *   addVertex, addEdge: O(1) | hasEdge, removeEdge: O(grau)
 *   bfs, dfs, hasPath, hasCycle, topologicalSort: O(V + E)
 *
 * O grafo é deliberadamente agnóstico ao domínio: não conhece tarefas,
 * dependências nem identificadores externos. Quem o utiliza é responsável
 * por traduzir seus próprios identificadores para índices de vértice.
 */
class Graph {
public:
    using Vertex = std::size_t;
    using Neighbors = std::vector<Vertex>;

    explicit Graph(std::size_t vertexCount, GraphType type = GraphType::Directed)
        : type_(type), adjacency_(vertexCount) {}

    // ------------------------------------------------------------------
    // Consultas básicas
    // ------------------------------------------------------------------

    [[nodiscard]] std::size_t vertexCount() const noexcept { return adjacency_.size(); }

    [[nodiscard]] std::size_t edgeCount() const noexcept { return edgeCount_; }

    [[nodiscard]] bool isDirected() const noexcept { return type_ == GraphType::Directed; }

    [[nodiscard]] const Neighbors& neighbors(Vertex vertex) const {
        validateVertex(vertex);
        return adjacency_[vertex];
    }

    [[nodiscard]] bool hasEdge(Vertex from, Vertex to) const {
        validateVertex(from);
        validateVertex(to);
        for (Vertex neighbor : adjacency_[from]) {
            if (neighbor == to) return true;
        }
        return false;
    }

    // ------------------------------------------------------------------
    // Modificação
    // ------------------------------------------------------------------

    void addEdge(Vertex from, Vertex to) {
        validateVertex(from);
        validateVertex(to);

        adjacency_[from].push_back(to);
        // Em grafos não direcionados a aresta é espelhada. Um laço (from == to)
        // é inserido apenas uma vez para não ser contado como aresta dupla.
        if (!isDirected() && from != to) {
            adjacency_[to].push_back(from);
        }
        ++edgeCount_;
    }

    /**
     * Acrescenta um vértice isolado e retorna seu índice. Permite que o
     * grafo cresça sob demanda, sem exigir o total de vértices na construção.
     */
    Vertex addVertex() {
        adjacency_.emplace_back();
        return adjacency_.size() - 1;
    }

    /**
     * Remove uma ocorrência da aresta from -> to (e a espelhada, em grafos
     * não direcionados). Em caso de arestas paralelas, as demais permanecem.
     *
     * @return true se a aresta existia e foi removida; false caso contrário.
     */
    bool removeEdge(Vertex from, Vertex to) {
        validateVertex(from);
        validateVertex(to);

        if (!eraseFirstOccurrence(adjacency_[from], to)) return false;

        // addEdge sempre insere o par espelhado, então ele necessariamente existe.
        if (!isDirected() && from != to) {
            eraseFirstOccurrence(adjacency_[to], from);
        }
        --edgeCount_;
        return true;
    }

    // ------------------------------------------------------------------
    // Alcançabilidade
    // ------------------------------------------------------------------

    /**
     * Indica se existe caminho de `from` até `to`. Todo vértice alcança a si
     * mesmo (caminho de comprimento zero). Útil para verificar, antes de
     * inserir a aresta u -> v, se ela fecharia um ciclo: isso ocorre
     * exatamente quando hasPath(v, u) é verdadeiro.
     */
    [[nodiscard]] bool hasPath(Vertex from, Vertex to) const {
        validateVertex(to);
        for (Vertex reachable : bfs(from)) {
            if (reachable == to) return true;
        }
        return false;
    }

    // ------------------------------------------------------------------
    // Busca em largura (BFS)
    // ------------------------------------------------------------------

    /**
     * Retorna os vértices alcançáveis a partir de `start`, na ordem em que
     * foram visitados pela busca em largura.
     */
    [[nodiscard]] std::vector<Vertex> bfs(Vertex start) const {
        validateVertex(start);

        std::vector<bool> visited(vertexCount(), false);
        std::vector<Vertex> visitOrder;
        std::queue<Vertex> pending;

        // Marcar ao enfileirar (e não ao desenfileirar) impede que um vértice
        // entre na fila mais de uma vez.
        visited[start] = true;
        pending.push(start);

        while (!pending.empty()) {
            const Vertex current = pending.front();
            pending.pop();
            visitOrder.push_back(current);

            for (Vertex neighbor : adjacency_[current]) {
                if (!visited[neighbor]) {
                    visited[neighbor] = true;
                    pending.push(neighbor);
                }
            }
        }
        return visitOrder;
    }

    // ------------------------------------------------------------------
    // Busca em profundidade (DFS)
    // ------------------------------------------------------------------

    /**
     * Retorna os vértices alcançáveis a partir de `start`, em pré-ordem
     * (mesma ordem de uma DFS recursiva clássica).
     */
    [[nodiscard]] std::vector<Vertex> dfs(Vertex start) const {
        validateVertex(start);

        std::vector<bool> visited(vertexCount(), false);
        std::vector<Vertex> visitOrder;
        std::vector<TraversalFrame> stack;

        visited[start] = true;
        visitOrder.push_back(start);
        stack.push_back({start, 0});

        while (!stack.empty()) {
            TraversalFrame& frame = stack.back();

            if (frame.nextNeighborIndex >= adjacency_[frame.vertex].size()) {
                stack.pop_back();
                continue;
            }

            const Vertex neighbor = adjacency_[frame.vertex][frame.nextNeighborIndex++];
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                visitOrder.push_back(neighbor);
                // `frame` pode ser invalidado por este push_back; não é usado depois.
                stack.push_back({neighbor, 0});
            }
        }
        return visitOrder;
    }

    // ------------------------------------------------------------------
    // Detecção de ciclos
    // ------------------------------------------------------------------

    /**
     * Indica se o grafo possui ao menos um ciclo (considera todos os
     * componentes, inclusive os desconexos).
     */
    [[nodiscard]] bool hasCycle() const {
        return isDirected() ? hasCycleDirected() : hasCycleUndirected();
    }

    // ------------------------------------------------------------------
    // Ordenação topológica
    // ------------------------------------------------------------------

    /**
     * Ordenação topológica pelo algoritmo de Kahn.
     *
     * @return Vértices em ordem topológica, ou std::nullopt se o grafo
     *         contém um ciclo (ordenação impossível).
     * @throws std::logic_error se o grafo for não direcionado, pois a
     *         ordenação topológica só é definida para grafos direcionados.
     */
    [[nodiscard]] std::optional<std::vector<Vertex>> topologicalSort() const {
        if (!isDirected()) {
            throw std::logic_error(
                "Graph::topologicalSort: ordenação topológica exige um grafo direcionado.");
        }

        std::vector<std::size_t> inDegree(vertexCount(), 0);
        for (const Neighbors& neighbors : adjacency_) {
            for (Vertex neighbor : neighbors) {
                ++inDegree[neighbor];
            }
        }

        std::queue<Vertex> ready;
        for (Vertex vertex = 0; vertex < vertexCount(); ++vertex) {
            if (inDegree[vertex] == 0) ready.push(vertex);
        }

        std::vector<Vertex> sorted;
        sorted.reserve(vertexCount());

        while (!ready.empty()) {
            const Vertex current = ready.front();
            ready.pop();
            sorted.push_back(current);

            for (Vertex neighbor : adjacency_[current]) {
                if (--inDegree[neighbor] == 0) ready.push(neighbor);
            }
        }

        // Se algum vértice nunca atingiu grau de entrada 0, ele pertence a um ciclo.
        if (sorted.size() != vertexCount()) return std::nullopt;
        return sorted;
    }

private:
    static constexpr Vertex kNoVertex = std::numeric_limits<Vertex>::max();

    // Estado de um vértice na pilha explícita que substitui a recursão.
    struct TraversalFrame {
        Vertex vertex;
        std::size_t nextNeighborIndex;
    };

    // Usado na detecção de ciclos em grafos não direcionados.
    struct UndirectedFrame {
        Vertex vertex;
        Vertex parent;
        std::size_t nextNeighborIndex;
        bool parentEdgeSkipped;
    };

    enum class Color { White, Gray, Black };

    // Preserva a ordem dos demais vizinhos, mantendo as travessias determinísticas.
    static bool eraseFirstOccurrence(Neighbors& neighbors, Vertex vertex) {
        const auto position = std::find(neighbors.begin(), neighbors.end(), vertex);
        if (position == neighbors.end()) return false;
        neighbors.erase(position);
        return true;
    }

    void validateVertex(Vertex vertex) const {
        if (vertex >= vertexCount()) {
            throw std::out_of_range("Graph: vértice " + std::to_string(vertex) +
                                    " inválido; o grafo possui " +
                                    std::to_string(vertexCount()) + " vértice(s).");
        }
    }

    /**
     * Grafo direcionado: há ciclo se, durante a DFS, encontramos uma aresta
     * para um vértice que ainda está na pilha de execução (cinza).
     */
    [[nodiscard]] bool hasCycleDirected() const {
        std::vector<Color> color(vertexCount(), Color::White);

        for (Vertex root = 0; root < vertexCount(); ++root) {
            if (color[root] != Color::White) continue;

            std::vector<TraversalFrame> stack;
            color[root] = Color::Gray;
            stack.push_back({root, 0});

            while (!stack.empty()) {
                TraversalFrame& frame = stack.back();

                if (frame.nextNeighborIndex >= adjacency_[frame.vertex].size()) {
                    color[frame.vertex] = Color::Black;
                    stack.pop_back();
                    continue;
                }

                const Vertex neighbor = adjacency_[frame.vertex][frame.nextNeighborIndex++];
                if (color[neighbor] == Color::Gray) return true;
                if (color[neighbor] == Color::White) {
                    color[neighbor] = Color::Gray;
                    stack.push_back({neighbor, 0});
                }
            }
        }
        return false;
    }

    /**
     * Grafo não direcionado: há ciclo se a DFS encontra um vértice já
     * visitado que não seja a aresta de volta ao pai. A aresta do pai é
     * ignorada apenas uma vez, para que arestas paralelas sejam detectadas
     * como ciclo.
     */
    [[nodiscard]] bool hasCycleUndirected() const {
        std::vector<bool> visited(vertexCount(), false);

        for (Vertex root = 0; root < vertexCount(); ++root) {
            if (visited[root]) continue;

            std::vector<UndirectedFrame> stack;
            visited[root] = true;
            stack.push_back({root, kNoVertex, 0, false});

            while (!stack.empty()) {
                UndirectedFrame& frame = stack.back();

                if (frame.nextNeighborIndex >= adjacency_[frame.vertex].size()) {
                    stack.pop_back();
                    continue;
                }

                const Vertex neighbor = adjacency_[frame.vertex][frame.nextNeighborIndex++];

                if (neighbor == frame.parent && !frame.parentEdgeSkipped) {
                    frame.parentEdgeSkipped = true;
                    continue;
                }
                if (visited[neighbor]) return true;

                visited[neighbor] = true;
                stack.push_back({neighbor, frame.vertex, 0, false});
            }
        }
        return false;
    }

    GraphType type_;
    std::vector<Neighbors> adjacency_;
    std::size_t edgeCount_ = 0;
};

}  // namespace Graph

#endif  // GRAPH
