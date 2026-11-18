// Testes simples para Graph (sem frameworks externos).
//
// Compilar e executar:
//   g++ -std=c++17 -Wall -Wextra GraphTests.cpp Graph.cpp -o graph_tests
//   ./graph_tests

#include "Graph.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

int testsRun = 0;
int testsFailed = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            ++testsFailed;                                                   \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond "\n"; \
        }                                                                    \
    } while (0)

#define CHECK_THROWS(expr, ExceptionType)                                    \
    do {                                                                     \
        bool thrown = false;                                                 \
        try {                                                                \
            expr;                                                            \
        } catch (const ExceptionType&) {                                     \
            thrown = true;                                                   \
        } catch (...) {                                                      \
        }                                                                    \
        if (!thrown) {                                                       \
            ++testsFailed;                                                   \
            std::cerr << "  FALHOU (linha " << __LINE__                      \
                      << "): esperava excecao " #ExceptionType " em " #expr  \
                      << "\n";                                               \
        }                                                                    \
    } while (0)

#define RUN(test)                                \
    do {                                         \
        ++testsRun;                              \
        std::cout << "[ RUN ] " #test "\n";      \
        test();                                  \
    } while (0)

using Order = std::vector<Graph::Vertex>;

// ---- Vértices --------------------------------------------------------------

void testAddAndHasVertex() {
    Graph g;
    CHECK(g.addVertex(1));
    CHECK(!g.addVertex(1)); // duplicado
    CHECK(g.hasVertex(1));
    CHECK(!g.hasVertex(2));
    CHECK(g.vertexCount() == 1);
}

void testRemoveVertexDirected() {
    Graph g(true);
    for (int v = 1; v <= 3; ++v) g.addVertex(v);
    g.addEdge(1, 2);
    g.addEdge(2, 3);
    g.addEdge(3, 2);

    CHECK(g.removeVertex(2)); // remove saída (2->3) e entradas (1->2, 3->2)
    CHECK(!g.removeVertex(2));
    CHECK(!g.hasVertex(2));
    CHECK(g.edgeCount() == 0);
    CHECK(g.getNeighbors(1).empty());
    CHECK(g.getNeighbors(3).empty());
}

void testRemoveVertexUndirected() {
    Graph g(false);
    for (int v = 1; v <= 3; ++v) g.addVertex(v);
    g.addEdge(1, 2);
    g.addEdge(2, 3);
    g.addEdge(1, 3);

    CHECK(g.removeVertex(2));
    CHECK(g.edgeCount() == 1);
    CHECK(g.hasEdge(1, 3));
    CHECK(g.getNeighbors(1) == Graph::NeighborList{3});
}

// ---- Arestas ---------------------------------------------------------------

void testAddEdgeDirected() {
    Graph g(true);
    g.addVertex(1);
    g.addVertex(2);

    CHECK(g.addEdge(1, 2));
    CHECK(!g.addEdge(1, 2)); // duplicada
    CHECK(g.hasEdge(1, 2));
    CHECK(!g.hasEdge(2, 1));
    CHECK(g.edgeCount() == 1);

    CHECK(g.addEdge(1, 1)); // laço permitido em dirigido
    CHECK(g.edgeCount() == 2);
}

void testAddEdgeUndirected() {
    Graph g(false);
    g.addVertex(1);
    g.addVertex(2);

    CHECK(g.addEdge(1, 2));
    CHECK(g.hasEdge(1, 2));
    CHECK(g.hasEdge(2, 1)); // simétrica
    CHECK(!g.addEdge(2, 1)); // já existe
    CHECK(g.edgeCount() == 1);
    CHECK_THROWS(g.addEdge(1, 1), std::invalid_argument); // laço proibido
}

void testAddEdgeMissingVertex() {
    Graph g;
    g.addVertex(1);
    CHECK_THROWS(g.addEdge(1, 99), std::invalid_argument);
    CHECK_THROWS(g.addEdge(99, 1), std::invalid_argument);
    CHECK(g.edgeCount() == 0);
}

void testRemoveEdge() {
    Graph d(true);
    d.addVertex(1);
    d.addVertex(2);
    d.addEdge(1, 2);
    CHECK(!d.removeEdge(2, 1)); // sentido contrário não existe
    CHECK(d.removeEdge(1, 2));
    CHECK(!d.removeEdge(1, 2));
    CHECK(d.edgeCount() == 0);

    Graph u(false);
    u.addVertex(1);
    u.addVertex(2);
    u.addEdge(1, 2);
    CHECK(u.removeEdge(2, 1)); // remove nos dois sentidos
    CHECK(!u.hasEdge(1, 2));
    CHECK(!u.hasEdge(2, 1));
    CHECK(u.edgeCount() == 0);
}

void testHasEdgeMissingVertex() {
    Graph g;
    g.addVertex(1);
    CHECK(!g.hasEdge(1, 2));
    CHECK(!g.hasEdge(2, 1));
}

// ---- Consultas -------------------------------------------------------------

void testGetNeighbors() {
    Graph g;
    for (int v = 1; v <= 3; ++v) g.addVertex(v);
    g.addEdge(1, 2);
    g.addEdge(1, 3);

    CHECK((g.getNeighbors(1) == Graph::NeighborList{2, 3}));
    CHECK(g.getNeighbors(2).empty());
    CHECK_THROWS(g.getNeighbors(99), std::invalid_argument);
}

void testGetDegree() {
    Graph d(true);
    for (int v = 1; v <= 3; ++v) d.addVertex(v);
    d.addEdge(1, 2);
    d.addEdge(3, 2);
    d.addEdge(2, 3);
    d.addEdge(2, 2); // laço conta 2
    CHECK(d.getDegree(2) == 5); // entrada: 1->2, 3->2, 2->2; saída: 2->3, 2->2
    CHECK(d.getDegree(1) == 1);
    CHECK_THROWS(d.getDegree(99), std::invalid_argument);

    Graph u(false);
    for (int v = 1; v <= 3; ++v) u.addVertex(v);
    u.addEdge(1, 2);
    u.addEdge(1, 3);
    CHECK(u.getDegree(1) == 2);
    CHECK(u.getDegree(2) == 1);
}

void testCountsAndIsDirected() {
    CHECK(Graph().isDirected());
    CHECK(!Graph(false).isDirected());

    Graph g;
    CHECK(g.vertexCount() == 0);
    CHECK(g.edgeCount() == 0);
}

void testClear() {
    Graph g;
    g.addVertex(1);
    g.addVertex(2);
    g.addEdge(1, 2);
    g.clear();
    CHECK(g.vertexCount() == 0);
    CHECK(g.edgeCount() == 0);
    CHECK(!g.hasVertex(1));
}

void testPrint() {
    Graph g;
    for (int v = 1; v <= 3; ++v) g.addVertex(v);
    g.addEdge(1, 2);
    g.addEdge(1, 3);

    std::ostringstream out;
    g.print(out);
    CHECK(out.str() == "1 -> 2 3\n2 ->\n3 ->\n");
}

// ---- Algoritmos ------------------------------------------------------------

void testHasCycleDirected() {
    Graph g(true);
    CHECK(!g.hasCycle()); // grafo vazio

    for (int v = 1; v <= 3; ++v) g.addVertex(v);
    g.addEdge(1, 2);
    g.addEdge(2, 3);
    g.addEdge(1, 3);
    CHECK(!g.hasCycle()); // DAG

    g.addEdge(3, 1);
    CHECK(g.hasCycle());

    Graph loop(true);
    loop.addVertex(1);
    loop.addEdge(1, 1);
    CHECK(loop.hasCycle()); // laço é ciclo
}

void testHasCycleUndirected() {
    Graph g(false);
    CHECK(!g.hasCycle()); // grafo vazio

    for (int v = 1; v <= 4; ++v) g.addVertex(v);
    g.addEdge(1, 2);
    g.addEdge(2, 3);
    CHECK(!g.hasCycle()); // floresta (4 isolado)

    g.addEdge(3, 1);
    CHECK(g.hasCycle());
}

void testTopologicalSort() {
    Graph g(true);
    for (int v = 1; v <= 4; ++v) g.addVertex(v);
    g.addEdge(1, 2);
    g.addEdge(1, 3);
    g.addEdge(2, 4);
    g.addEdge(3, 4);

    CHECK((g.topologicalSort() == Order{1, 2, 3, 4}));
}

void testTopologicalSortErrors() {
    Graph cyclic(true);
    cyclic.addVertex(1);
    cyclic.addVertex(2);
    cyclic.addEdge(1, 2);
    cyclic.addEdge(2, 1);
    CHECK_THROWS(cyclic.topologicalSort(), std::logic_error);

    Graph undirected(false);
    undirected.addVertex(1);
    CHECK_THROWS(undirected.topologicalSort(), std::logic_error);

    Graph empty(true);
    CHECK(empty.topologicalSort().empty());
}

} // namespace

int main() {
    RUN(testAddAndHasVertex);
    RUN(testRemoveVertexDirected);
    RUN(testRemoveVertexUndirected);
    RUN(testAddEdgeDirected);
    RUN(testAddEdgeUndirected);
    RUN(testAddEdgeMissingVertex);
    RUN(testRemoveEdge);
    RUN(testHasEdgeMissingVertex);
    RUN(testGetNeighbors);
    RUN(testGetDegree);
    RUN(testCountsAndIsDirected);
    RUN(testClear);
    RUN(testPrint);
    RUN(testHasCycleDirected);
    RUN(testHasCycleUndirected);
    RUN(testTopologicalSort);
    RUN(testTopologicalSortErrors);

    std::cout << "\n" << testsRun << " testes executados, "
              << testsFailed << " verificacao(oes) falhou(aram).\n";
    return testsFailed == 0 ? 0 : 1;
}
