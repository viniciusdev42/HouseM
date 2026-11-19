#ifndef TRIE_HPP
#define TRIE_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "src/Model/Entities/Task/Task.hpp"

/**
 * Trie (árvore de prefixos) que associa palavras a identificadores de tarefas.
 *
 * Cada nó guarda o seu caractere e, quando uma palavra termina nele, a lista
 * de TaskIds associados a essa palavra. Uma palavra "existe" na Trie enquanto
 * possuir ao menos um TaskId.
 *
 * Um índice reverso (TaskId -> palavras) acompanha todas as mutações, de modo
 * que remover ou editar uma tarefa não exige varrer a árvore inteira.
 *
 * Complexidades (L = tamanho da palavra/prefixo, T = ids por palavra,
 * W = palavras da tarefa):
 *  - insert / remove / findTaskIds / search / startsWith: O(L + T)
 *  - removeTask: O(W * (L + T))
 *  - autocomplete: O(L + K), onde K é o número de nós visitados na subárvore
 *
 * Observações:
 *  - Os ids são do tipo Task::TaskId (Value Object definido em Task.hpp). Como ele
 *    não possui std::hash, a Trie usa um hasher próprio (TaskIdHash) sobre value().
 *    Comparar/hashear um id custa O(|id|).
 *  - A comparação é feita byte a byte (case-sensitive).
 *  - Os filhos ficam em std::map para que o autocomplete retorne os resultados
 *    em ordem lexicográfica determinística.
 *  - A classe é move-only, pois é dona exclusiva dos seus nós.
 *  - Não é thread-safe; a sincronização é responsabilidade de quem usa.
 */
class Trie {
public:
    Trie();
    ~Trie() = default;

    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;
    Trie(Trie&&) noexcept = default;
    Trie& operator=(Trie&&) noexcept = default;

    /**
     * Associa um TaskId a uma palavra, criando a palavra se necessário.
     * @return true se a associação era nova; false se já existia.
     * @throws std::invalid_argument se a palavra for vazia.
     */
    bool insert(std::string_view word, const Task::TaskId& taskId);

    /**
     * Remove a associação entre a palavra e o TaskId. Se a palavra ficar sem
     * nenhum TaskId, ela deixa de existir e os nós inúteis são podados.
     * @return true se a associação existia e foi removida.
     */
    bool remove(std::string_view word, const Task::TaskId& taskId);

    /**
     * Remove a palavra com todos os TaskIds associados a ela.
     * @return true se a palavra existia.
     */
    bool removeWord(std::string_view word);

    /**
     * Remove todas as associações de uma tarefa (uso: tarefa excluída).
     * @return true se a tarefa estava indexada.
     */
    bool removeTask(const Task::TaskId& taskId);

    /**
     * Substitui o conjunto de palavras de uma tarefa (uso: tarefa editada).
     * Remove as palavras que saíram, adiciona as novas e preserva as demais.
     * Uma lista vazia equivale a removeTask(taskId).
     * @throws std::invalid_argument se alguma palavra for vazia (nada é alterado).
     */
    void updateTask(const Task::TaskId& taskId, const std::vector<std::string>& newWords);

    /** Retorna true se a palavra exata está armazenada. */
    bool search(std::string_view word) const;

    /**
     * Retorna true se existe ao menos uma palavra que comece com o prefixo.
     * Um prefixo vazio retorna true somente se a Trie não estiver vazia.
     */
    bool startsWith(std::string_view prefix) const;

    /** Retorna os TaskIds associados à palavra (vazio se ela não existir). */
    std::vector<Task::TaskId> findTaskIds(std::string_view word) const;

    /** Retorna as palavras indexadas para a tarefa (vazio se ela não existir). */
    std::vector<std::string> wordsOf(const Task::TaskId& taskId) const;

    /**
     * Retorna as palavras que começam com o prefixo, em ordem lexicográfica.
     * @param prefix     Prefixo a ser completado (pode ser vazio para listar tudo).
     * @param maxResults Limite de resultados; 0 significa sem limite.
     */
    std::vector<std::string> autocomplete(std::string_view prefix,
                                          std::size_t maxResults = 0) const;

    /** Quantidade de palavras distintas armazenadas. */
    std::size_t size() const noexcept;

    /** Retorna true se não há palavras armazenadas. */
    bool empty() const noexcept;

    /** Remove todas as palavras e o índice reverso. */
    void clear();

private:
    /** Hasher para Task::TaskId, baseado na string interna do id. */
    struct TaskIdHash {
        std::size_t operator()(const Task::TaskId& id) const noexcept {
            return std::hash<std::string>{}(id.value());
        }
    };

    struct Node {
        explicit Node(char c = '\0') : character(c) {}

        // Uma palavra termina no nó se, e somente se, houver TaskIds nele.
        // Derivar isso da lista evita uma flag que poderia ficar inconsistente.
        bool hasWord() const noexcept { return !taskIds.empty(); }

        char character;
        std::map<char, std::unique_ptr<Node>> children;
        std::vector<Task::TaskId> taskIds;
    };

    /** Retorna o nó correspondente ao prefixo, ou nullptr se não existir. */
    const Node* findNode(std::string_view prefix) const;
    Node* findNode(std::string_view prefix);

    /** Remove, de baixo para cima, os nós sem palavra e sem filhos no caminho. */
    void pruneBranch(std::string_view word);

    /** Remove a palavra da lista do índice reverso da tarefa. */
    void unlinkFromReverseIndex(const Task::TaskId& taskId, std::string_view word);

    /** Coleta, em DFS, as palavras da subárvore respeitando o limite. */
    void collectWords(const Node* node,
                      std::string& currentWord,
                      std::vector<std::string>& results,
                      std::size_t maxResults) const;

    std::unique_ptr<Node> root_;
    std::size_t wordCount_ = 0;
    std::unordered_map<Task::TaskId, std::vector<std::string>, TaskIdHash> reverseIndex_;
};

#endif // TRIE_HPP 