#ifndef DEPENDENCY_SERVICE
#define DEPENDENCY_SERVICE

#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/Model/Entities/Task/Task.hpp"
#include "src/Model/Entities/Dependency/Dependency.hpp"
#include "src/Model/Structures/Graph/Graph.hpp"

namespace Dependency {

/* Serviço que gerencia as dependências entre tarefas.

   O grafo (Graph) trabalha com vértices inteiros, enquanto o domínio usa
   TaskId (UUID). O serviço mantém o mapeamento entre os dois e traduz todas
   as operações, de modo que quem o usa enxerga apenas TaskId e Dependency.

   Convenção do grafo: a aresta  pré-requisito -> dependente  representa a
   dependência. Logo, uma ordenação topológica lista os pré-requisitos antes
   das tarefas que dependem deles.

   Invariante: o grafo é sempre acíclico. addDependency rejeita qualquer
   dependência que fecharia um ciclo.

   Apenas tarefas que participam de ao menos uma dependência são rastreadas;
   uma tarefa sem dependências não possui vértice e é removida
   automaticamente quando perde sua última dependência.
*/
class DependencyService {
public:
    using TaskId = Task::TaskId;

    DependencyService() : graph_(true) {}

    // --- Escrita -----------------------------------------------------------

    /// Registra que `dependentId` depende de `prerequisiteId`.
    /// @return false se a dependência já existia.
    /// @throws std::invalid_argument se o par for inválido (ex.: mesma tarefa).
    /// @throws std::logic_error se a dependência criaria um ciclo.
    bool addDependency(const TaskId& prerequisiteId, const TaskId& dependentId);
        
    /// Remove a dependência. Tarefas que ficarem sem nenhuma dependência
    /// deixam de ser rastreadas.
    /// @return false se a dependência não existia.
    
    bool removeDependency(const TaskId& prerequisiteId, const TaskId& dependentId);
    

    /// Remove todas as dependências em que a tarefa participa (como
    /// pré-requisito ou como dependente). Útil ao excluir uma tarefa.
    /// @return quantidade de dependências removidas.
    
    std::size_t removeTaskDependencies(const TaskId& taskId);
    
    // --- Consultas ---------------------------------------------------------

    /// Dependências diretas da tarefa: um item para cada pré-requisito dela
    /// (isto é, as arestas que chegam em `taskId`).
    
    std::vector<Dependency> getDependencies(const TaskId& taskId) const;
     
    /// Dependências em que a tarefa é o pré-requisito: um item para cada
    /// tarefa que ela bloqueia diretamente (arestas que saem de `taskId`).
    
    std::vector<Dependency> getDependents(const TaskId& taskId) const;
    
    /// Indica se existe a dependência direta `prerequisiteId` -> `dependentId`.
    
    bool hasDependency(const TaskId& prerequisiteId, const TaskId& dependentId) const;
      
    /// Indica se `to` é alcançável a partir de `from` seguindo dependências
    /// (dependência direta ou transitiva). Uma tarefa rastreada sempre
    /// alcança a si mesma.
    
    bool hasPath(const TaskId& from, const TaskId& to) const;
    
    /// Indica se o conjunto atual de dependências contém ciclo.
    /// Como addDependency preserva o invariante, normalmente retorna false.
    
    bool detectCycle() const;
 
    /// Indica se adicionar `prerequisiteId` -> `dependentId` criaria um ciclo,
    /// sem alterar o estado do serviço.
    
    bool detectCycle(const TaskId& prerequisiteId, const TaskId& dependentId) const;
 
    /// Tarefas bloqueadas: ainda não concluídas e com ao menos um
    
    /// pré-requisito fora de `completedTasks`.
    
    std::vector<TaskId> getBlockedTasks(const std::set<TaskId>& completedTasks) const;
    
    /// Tarefas disponíveis: ainda não concluídas e com todos os
    /// pré-requisitos em `completedTasks`. Considera somente tarefas que
    /// participam de alguma dependência.
   
   std::vector<TaskId> getAvailableTasks(const std::set<TaskId>& completedTasks) const;
    
    /// Alias com a grafia usada no enunciado ("Avaliable").
    
    std::vector<TaskId> getAvaliableTasks(const std::set<TaskId>& completedTasks) const;
 
    /// Ordem válida de execução: todo pré-requisito aparece antes de seus
    /// dependentes.
    /// @throws std::logic_error se houver ciclo (não ocorre se o invariante
    ///         for preservado).
 
  std::vector<TaskId> getTopologicalOrder() const;
     
private:
    using Vertex = Graph::Vertex;

    /// Retorna o vértice da tarefa, criando-o se ainda não existir.
    
    Vertex ensureVertex(const TaskId& id);
    
    void eraseVertex(Vertex vertex);
      
    void eraseIfIsolated(Vertex vertex);
  
    /// Busca em profundidade iterativa (evita estouro de pilha em cadeias longas).
   
   bool hasPathBetween(Vertex from, Vertex to) const;
   

    /// Para cada tarefa rastreada, conta os pré-requisitos ainda não concluídos.
    
    std::unordered_map<Vertex, std::size_t> countPendingPrerequisites(const std::set<TaskId>& completedTasks) const;
    
    Graph graph_;
    std::map<TaskId, Vertex> ids_;
    std::map<Vertex, TaskId> tasks_;
    Vertex nextVertex_ = 0;  // nunca reutilizado, para não confundir vértices antigos com novos
};

}  // namespace Dependency

#endif  // DEPENDENCY_SERVICE
