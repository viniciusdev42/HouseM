#ifndef SEARCH_SERVICE_HPP
#define SEARCH_SERVICE_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "src/Model/Repositories/ITaskRepository.hpp"
#include "src/Model/Structures/Trie/Trie.hpp"
#include"src/Model/Structures/String/Algorithms/Kmp/Kmp.hpp"
#include "src/Model/Structures/String/Algorithms/Karp/RabinKarp.hpp"
#include "src/Model/Structures/String/Algorithms/Levenshtein/Levenshtein.hpp"
#include "src/Model/Structures/String/Normalization/Normalizer.hpp"

namespace SearchService {

enum class SortField { Title, Priority, Status, Deadline };
enum class SortOrder { Ascending, Descending };
enum class SubstringScope { Title, Description, Both };

// Como combinar vários padrões em searchByPatterns.
enum class PatternMatchMode {
    Any,  // a tarefa casa se QUALQUER padrão aparecer (OU)
    All   // a tarefa casa somente se TODOS os padrões aparecerem (E)
};

struct SortSpec {
    SortField field = SortField::Title;
    SortOrder order = SortOrder::Ascending;
};

class SearchService {
public:
    using Repository = ITaskRepository::ITaskRepository;
    using TextNormalizer = Normalizer::Normalizer
    using TimePoint = Repository::TimePoint;

    explicit SearchService(const Repository& repository, const TextNormalizer& normalizer);

    std::optional<Task> searchById(const TaskId& id) const;
    std::vector<Task> searchByTitle(const std::string& term) const;
    std::vector<Task> searchByDescription(const std::string& term) const;
    std::vector<Task> searchByStatus(Status status) const;
    std::vector<Task> searchByPriority(Priority priority) const;

    // Intervalo fechado [from, to], como antes da refatoração.
    std::vector<Task> searchByDeadline(TimePoint from, TimePoint to) const;

    // Intervalo fechado de prioridades [from, to]. Por padrão, ordena por
    // prioridade crescente.
    std::vector<Task> searchByPriorityRange(
        Priority from, Priority to,
        SortSpec spec = {SortField::Priority, SortOrder::Ascending}) const;

    // Intervalo lexicográfico fechado de títulos [from, to], sem distinguir
    // maiúsculas de minúsculas. Por padrão, ordena por título crescente.
    std::vector<Task> searchByTitleRange(
        const std::string& from, const std::string& to,
        SortSpec spec = {SortField::Title, SortOrder::Ascending}) const;

     // Pesquisa por substring (KMP), sem distinguir maiúsculas de minúsculas.
    // Com SubstringScope::Both, a tarefa casa se o termo aparecer no título
    // OU na descrição. Por padrão, mantém a ordem retornada pelo repositório.
    std::vector<Task> searchBySubstring(const std::string& term,
                                        SubstringScope scope = SubstringScope::Both,
                                        SortSpec spec = {}) const;

      // Pesquisa textual por padrão (Rabin-Karp), sem distinguir maiúsculas de
    // minúsculas. Lança std::invalid_argument se o padrão for vazio.
    // O resultado é ordenado segundo 'spec' (padrão: título crescente).
    std::vector<Task> searchByPattern(const std::string& pattern,
                                      SubstringScope scope = SubstringScope::Both,
                                      SortSpec spec = {}) const;

    // Pesquisa por vários padrões (Rabin-Karp) combinados por 'mode'.
    // Com SubstringScope::Both, cada padrão é procurado no título OU na
    // descrição. Lança std::invalid_argument se a lista ou algum padrão
    // for vazio.
    std::vector<Task> searchByPatterns(const std::vector<std::string>& patterns,
                                       PatternMatchMode mode = PatternMatchMode::Any,
                                       SubstringScope scope = SubstringScope::Both,
                                       SortSpec spec = {}) const;

    // Todas as tarefas, ordenadas segundo 'spec'.
    std::vector<Task> searchAll(SortSpec spec = {}) const;

    // Ordena (de forma estável) qualquer resultado de busca. Em Deadline,
    // tarefas sem prazo ficam sempre no final, em ambas as ordens.
    static void sortTasks(std::vector<Task>& tasks, SortSpec spec);

    std::vector<std::string> autocomplete(const std::string& prefix,
                                          std::size_t maxResults = 0) const;

    // Sugestões de pesquisa ("você quis dizer?"). Compara 'term' com o
    // vocabulário indexado (palavras de títulos e descrições), sem distinguir
    // maiúsculas de minúsculas. Candidatos aceitos:
    //   - palavras que começam com 'term' (completamento), ou
    //   - palavras com similaridade de Levenshtein >= minSimilarity.
    // Ordem: completamentos primeiro; depois menor distância; depois ordem
    // alfabética. maxResults == 0 significa sem limite (como em autocomplete).
    // Lança std::invalid_argument se 'term' for vazio ou se minSimilarity
    // estiver fora de [0, 1]. Nunca sugere o próprio termo.
    std::vector<std::string> suggest(const std::string& term,
                                     std::size_t maxResults = 5,
                                     double minSimilarity = 0.5) const;

    void indexTask(const Task& task);
    void removeTaskFromIndex(const TaskId& id);
    void rebuildIndex();

private:
    // Aplica normalizer_.normalize() e devolve o resultado como std::string.
    std::string normalize(const std::string& text) const;

    // Valida (não vazio, antes e depois de normalizar) e normaliza um termo
    // de busca. 'operation' só alimenta a mensagem de erro.
    std::string normalizeTerm(const std::string& term, const char* operation) const;

    std::vector<std::string> extractIndexWords(const Task& task) const;

    const Repository& repository_;
    const TextNormalizer& normalizer_;
    Trie index_;
};

}  // namespace SearchService
#endif // SEARCH_SERVICE_HPP