#ifndef DEPENDENCY_SERVICE_HPP
#define DEPENDENCY_SERVICE_HPP

#include <cstddef>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include "src/Model/Repositories/ITaskRepository.hpp"
#include "src/Model/Entities/Dependency/Dependency.hpp"
#include "src/Model/Structures/Graph/Graph.hpp"

namespace DependencyService {

/* Serviço que gerencia as dependências entre tarefas.

   Convenção do grafo: a aresta  pré-requisito -> dependente  representa a
   dependência. Invariante: o grafo é sempre acíclico.

   Decisões-chave
   - Injeção de dependência: o serviço recebe um ITaskRepository (abstração),
     não a implementação em memória. Qualquer implementação serve.
   - O repositório é a fonte da verdade das tarefas: addDependency exige que
     ambas existam (TaskNotFoundError) e as consultas que devolvem tarefas
     (bloqueadas, disponíveis, ordem topológica) as materializam via
     findById, retornando Task em vez de TaskId.
   - O grafo continua guardando só ids/vértices; o repositório não é
     consultado nas operações de remoção e nas consultas que devolvem
     Dependency, para que seja possível limpar dependências de uma tarefa já
     excluída do repositório.
   - Tarefa inexistente no repositório durante uma consulta é erro
     (TaskNotFoundError), nunca é omitida silenciosamente.

   Débitos técnicos
   - Sem sincronização automática: excluir uma tarefa do repositório não
     remove suas dependências. O chamador deve usar removeTaskDependencies
     (ou o ideal seria um observer/evento de remoção no repositório).
   - O serviço guarda referência ao repositório: ele precisa sobreviver ao
     serviço.
   - O serviço não é thread-safe (o repositório é). Há janela de corrida
     entre a checagem de existência e a inserção da aresta.
   - eraseIfIsolated assume que Graph::getDegree soma arestas de entrada e
     saída.
   - getDependencies, removeTaskDependencies e a contagem de pendências
     varrem todos os vértices (O(V)) por falta de consulta de predecessores
     no Graph.
   - Cada materialização de Task faz uma cópia/consulta no repositório
     (N chamadas a findById); não há consulta em lote.
*/
class DependencyService {
public:
    using TaskId = ::TaskId;
    using TaskRepository = ::ITaskRepository::ITaskRepository;

    /// O repositório deve sobreviver ao serviço.
    explicit DependencyService(const TaskRepository& repository);

    // --- Escrita -----------------------------------------------------------

    /// Registra que `dependentId` depende de `prerequisiteId`.
    /// @return false se a dependência já existia.
    /// @throws std::invalid_argument se o par for inválido (ex.: mesma tarefa).
    /// @throws ITaskRepository::TaskNotFoundError se alguma tarefa não existir
    ///         no repositório.
    /// @throws std::logic_error se a dependência criaria um ciclo.
    bool addDependency(const TaskId& prerequisiteId, const TaskId& dependentId);

    /// Remove a dependência. Tarefas que ficarem sem nenhuma dependência
    /// deixam de ser rastreadas.
    /// @return false se a dependência não existia.
    bool removeDependency(const TaskId& prerequisiteId, const TaskId& dependentId);

    /// Remove todas as dependências em que a tarefa participa.
    /// @return quantidade de dependências removidas.
    std::size_t removeTaskDependencies(const TaskId& taskId);

    // --- Consultas ---------------------------------------------------------

    /// Um item para cada pré-requisito direto de `taskId` (arestas que chegam).
    std::vector<Dependency> getDependencies(const TaskId& taskId) const;

    /// Um item para cada tarefa que `taskId` bloqueia diretamente (arestas que saem).
    std::vector<Dependency> getDependents(const TaskId& taskId) const;

    /// Existe a dependência direta `prerequisiteId` -> `dependentId`?
    bool hasDependency(const TaskId& prerequisiteId, const TaskId& dependentId) const;

    /// `to` é alcançável a partir de `from` (dependência direta ou transitiva)?
    bool hasPath(const TaskId& from, const TaskId& to) const;

    /// O conjunto atual de dependências contém ciclo? (normalmente false)
    bool detectCycle() const;

    /// Adicionar `prerequisiteId` -> `dependentId` criaria um ciclo?
    /// Não altera o estado do serviço.
    bool detectCycle(const TaskId& prerequisiteId, const TaskId& dependentId) const;

    /// Tarefas não concluídas com ao menos um pré-requisito fora de `completedTasks`.
    /// @throws ITaskRepository::TaskNotFoundError se uma tarefa rastreada
    ///         não existir mais no repositório.
    std::vector<Task> getBlockedTasks(const std::set<TaskId>& completedTasks) const;

    /// Tarefas não concluídas com todos os pré-requisitos em `completedTasks`.
    /// Considera somente tarefas que participam de alguma dependência.
    /// @throws ITaskRepository::TaskNotFoundError (idem getBlockedTasks).
    std::vector<Task> getAvailableTasks(const std::set<TaskId>& completedTasks) const;

    /// Alias com a grafia usada no enunciado ("Avaliable").
    std::vector<Task> getAvaliableTasks(const std::set<TaskId>& completedTasks) const;

    /// Ordem válida de execução: pré-requisitos antes de seus dependentes.
    /// @throws ITaskRepository::TaskNotFoundError (idem getBlockedTasks).
    /// @throws std::logic_error se houver ciclo (não ocorre com o invariante).
    std::vector<Task> getTopologicalOrder() const;

private:
    using Vertex = Graph::Vertex;

    /// Falha com TaskNotFoundError se a tarefa não existir no repositório.
    void requireTaskExists(const TaskId& id) const;

    /// Obtém a tarefa do repositório; lança TaskNotFoundError se não existir.
    Task loadTask(const TaskId& id) const;

    /// Retorna o vértice da tarefa, criando-o se ainda não existir.
    Vertex ensureVertex(const TaskId& id);

    void eraseVertex(Vertex vertex);

    void eraseIfIsolated(Vertex vertex);

    /// Busca em profundidade iterativa (evita estouro de pilha em cadeias longas).
    bool hasPathBetween(Vertex from, Vertex to) const;

    /// Para cada tarefa rastreada, conta os pré-requisitos ainda não concluídos.
    std::unordered_map<Vertex, std::size_t>
    countPendingPrerequisites(const std::set<TaskId>& completedTasks) const;

    /// Carrega as tarefas não concluídas cuja contagem de pendências satisfaz `keep`.
    template <typename Predicate>
    std::vector<Task> collectUncompleted(const std::set<TaskId>& completedTasks,
                                         Predicate keep) const;

    const TaskRepository& repository_;
    Graph graph_;
    std::map<TaskId, Vertex> ids_;
    std::map<Vertex, TaskId> tasks_;
    Vertex nextVertex_ = 0;  // nunca reutilizado, para não confundir vértices antigos com novos
};

}  // namespace DependencyService

#endif  // DEPENDENCY_SERVICE_HPP