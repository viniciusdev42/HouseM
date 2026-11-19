#ifndef SEARCH_SERVICE_HPP
#define SEARCH_SERVICE_HPP

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "src/Model/Repositories/ITaskRepository.hpp"
#include "src/Model/Structures/Trie/Trie.hpp"
#include "src/Model/Structures/String/Algorithms/Kmp/Kmp.hpp"
#include "src/Model/Structures/String/Algorithms/Karp/RabinKarp.hpp"
#include "src/Model/Structures/String/Algorithms/Levenshtein/Levenshtein.hpp"
#include "src/Model/Structures/String/Normalization/Normalizer.hpp"
#include "src/Model/Structures/String/Tokenization/Tokenizer.hpp"

namespace SearchService {

enum class SortField { Title, Priority, Status, Deadline };
enum class SortOrder { Ascending, Descending };
enum class SubstringScope { Title, Description, Both };

// Como combinar vários padrões/termos nas buscas multi-termo.
enum class PatternMatchMode {
    Any,  // a tarefa casa se QUALQUER padrão/termo aparecer (OU)
    All   // a tarefa casa somente se TODOS os padrões/termos aparecerem (E)
};

struct SortSpec {
    SortField field = SortField::Title;
    SortOrder order = SortOrder::Ascending;
};

class SearchService {
public:
    using Task = ::Task::Task;
    using TaskId = ::Task::TaskId;
    using Repository = ::ITaskRepository::ITaskRepository;
    using TextNormalizer = ::Normalizer::Normalizer;
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
    // OU na descrição. O resultado é ordenado segundo 'spec'.
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

    // Pesquisa por termos usando o índice invertido. Fluxo: normaliza a
    // consulta -> Tokenizer separa em tokens -> cada token é procurado
    // (igualdade exata) nas listas de ocorrências. Título e descrição são
    // indexados juntos. 'mode' combina os tokens (padrão: All, ou seja, a
    // tarefa precisa conter todos). Lança std::invalid_argument se a consulta
    // for vazia ou não tiver conteúdo pesquisável.
    std::vector<Task> searchByTerms(const std::string& query,
                                    PatternMatchMode mode = PatternMatchMode::All,
                                    SortSpec spec = {}) const;

    // Igual a searchByTerms, mas cada token da consulta é tratado como
    // PREFIXO: a Trie expande o prefixo nos tokens indexados que o começam,
    // e a união das listas de ocorrências desses tokens é o conjunto do
    // prefixo. Ex.: "reun jo" (All) encontra "reunião com João".
    std::vector<Task> searchByPrefix(const std::string& query,
                                     PatternMatchMode mode = PatternMatchMode::All,
                                     SortSpec spec = {}) const;

    // Todas as tarefas, ordenadas segundo 'spec'.
    std::vector<Task> searchAll(SortSpec spec = {}) const;

    // Ordena (de forma estável) qualquer resultado de busca. Em Deadline,
    // tarefas sem prazo ficam sempre no final, em ambas as ordens.
    static void sortTasks(std::vector<Task>& tasks, SortSpec spec);

    // Autocompletar. O texto é normalizado e tokenizado; o ÚLTIMO token é o
    // prefixo consultado na Trie (ex.: "reunião com jo" completa "jo").
    // Se o texto termina em espaço, não há palavra em digitação e o
    // resultado é vazio. Lança std::invalid_argument se o texto for vazio ou
    // não tiver conteúdo pesquisável. maxResults == 0 significa sem limite.
    std::vector<std::string> autocomplete(const std::string& prefix,
                                          std::size_t maxResults = 0) const;

    // Sugestões de pesquisa ("você quis dizer?"). Normaliza e tokeniza
    // 'term' (usa o último token) e o compara com o vocabulário indexado
    // (tokens de títulos e descrições). Candidatos aceitos:
    //   - palavras que começam com o termo (completamento), ou
    //   - palavras com similaridade de Levenshtein >= minSimilarity.
    // Ordem: completamentos primeiro; depois menor distância; depois ordem
    // alfabética. maxResults == 0 significa sem limite (como em autocomplete).
    // Lança std::invalid_argument se 'term' for vazio, não tiver conteúdo
    // pesquisável ou se minSimilarity estiver fora de [0, 1]. Nunca sugere o
    // próprio termo.
    std::vector<std::string> suggest(const std::string& term,
                                     std::size_t maxResults = 5,
                                     double minSimilarity = 0.5) const;

    void indexTask(const Task& task);
    void removeTaskFromIndex(const TaskId& id);
    void rebuildIndex();

private:
    // Conjunto ordenado de ids (exige operator< em TaskId).
    using IdSet = std::set<TaskId>;

    // Aplica normalizer_.normalize() e devolve o resultado como std::string.
    std::string normalize(const std::string& text) const;

    // Valida (não vazio, antes e depois de normalizar) e normaliza um termo
    // de busca. 'operation' só alimenta a mensagem de erro.
    std::string normalizeTerm(const std::string& term, const char* operation) const;

    // normalize() seguido de Tokenizer. Pode devolver lista vazia.
    std::vector<std::string> tokenize(const std::string& text) const;

    // Valida e tokeniza uma consulta; devolve tokens sem repetição.
    std::vector<std::string> queryTokens(const std::string& query, const char* operation) const;

    // Último token da consulta (a palavra "em digitação").
    std::string activeToken(const std::string& text, const char* operation) const;

    // Palavras a indexar de uma tarefa: tokens (normalizados) de título e
    // descrição, mais as partes de tokens compostos ("guarda-chuva" ->
    // "guarda-chuva", "guarda", "chuva"). Sem repetições.
    std::vector<std::string> extractIndexWords(const Task& task) const;

    // Índice invertido: token -> ids das tarefas que o contêm.
    IdSet idsForToken(const std::string& token) const;
    // Consulta a Trie pelo prefixo e une as ocorrências de todos os tokens.
    IdSet idsForPrefix(const std::string& prefix) const;
    std::vector<Task> tasksWithIds(const IdSet& ids, SortSpec spec) const;

    void addToPostings(const TaskId& id, const std::vector<std::string>& words);
    void removeFromPostings(const TaskId& id);

    const Repository& repository_;
    const TextNormalizer& normalizer_;
    ::Tokenizer::Tokenizer tokenizer_;
    Trie index_;                              // vocabulário (autocomplete/suggest)
    std::map<std::string, IdSet> postings_;   // token -> tarefas (índice invertido)
};

}  // namespace SearchService
#endif // SEARCH_SERVICE_HPP
