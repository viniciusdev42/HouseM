#ifndef GRAPH
#define GRAPH

#include <cstddef>
#include <iostream>
#include <map>
#include <vector>

/**
 * Grafo implementado com Lista de Adjacência.
 *
 * - Vértices são identificados por inteiros.
 * - Pode ser dirigido (padrão) ou não dirigido, definido na construção.
 * - Não admite arestas paralelas. Laços (u -> u) só são permitidos em grafos dirigidos.
 * - Os vértices são mantidos ordenados (std::map), o que torna a saída de
 *   print(), topologicalSort() e demais algoritmos determinística.
 */
class Graph {
public:
    using Vertex = int;
    using NeighborList = std::vector<Vertex>;

    explicit Graph(bool directed = true);

    // ---- Vértices ----------------------------------------------------------
    /** Retorna false se o vértice já existia. */
    bool addVertex(Vertex v);

    /** Remove o vértice e todas as arestas incidentes. Retorna false se não existia. */
    bool removeVertex(Vertex v);

    bool hasVertex(Vertex v) const;

    // ---- Arestas -----------------------------------------------------------
    /**
     * Retorna false se a aresta já existia.
     * @throws std::invalid_argument se algum vértice não existir ou se for
     *         um laço em grafo não dirigido.
     */
    bool addEdge(Vertex from, Vertex to);

    /** Retorna false se a aresta (ou algum dos vértices) não existia. */
    bool removeEdge(Vertex from, Vertex to);

    /** Retorna false se a aresta (ou algum dos vértices) não existir. */
    bool hasEdge(Vertex from, Vertex to) const;

    // ---- Consultas ---------------------------------------------------------
    /**
     * Vizinhos de saída de v (em grafos não dirigidos, todos os vizinhos).
     * @throws std::invalid_argument se v não existir.
     */
    const NeighborList& getNeighbors(Vertex v) const;

    /**
     * Grau de v.
     *  - Não dirigido: número de vizinhos.
     *  - Dirigido: grau de entrada + grau de saída (laço conta 2).
     * @throws std::invalid_argument se v não existir.
     */
    std::size_t getDegree(Vertex v) const;

    std::size_t vertexCount() const noexcept;
    std::size_t edgeCount() const noexcept;
    bool isDirected() const noexcept;

    void clear() noexcept;

    /** Imprime a lista de adjacência, uma linha por vértice: "v -> a b c". */
    void print(std::ostream& os = std::cout) const;

    // ---- Algoritmos --------------------------------------------------------
    /** Detecta ciclos (dirigido: DFS com cores; não dirigido: contagem de componentes). */
    bool hasCycle() const;

    /**
     * Ordenação topológica (algoritmo de Kahn).
     * @throws std::logic_error se o grafo não for dirigido ou contiver ciclo.
     */
    std::vector<Vertex> topologicalSort() const;

private:
    bool directed_;
    std::size_t edgeCount_;
    std::map<Vertex, NeighborList> adjacency_;

    const NeighborList& requireVertex(Vertex v) const;
    bool hasCycleDirected() const;
    bool hasCycleUndirected() const;
};

#endif // GRAPH
