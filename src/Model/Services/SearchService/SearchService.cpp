#include "SearchService.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace SearchService {

namespace {

// Único ponto que conhece a conversão std::string -> KView.
// Se o construtor de StringView for diferente, só esta função muda.
KView toView(const std::string& text) {
    return KView(text.data(), text.size());
}

// Ambos os argumentos já devem estar normalizados (o chamador normaliza uma
// única vez por tarefa, e não uma vez por padrão).
bool kmpContains(const std::string& normalizedText, const std::string& normalizedTerm) {
    return KMP::search(toView(normalizedText), toView(normalizedTerm)).size() > 0;
}

// Ambos os argumentos já devem estar estar normalizados
bool rabinKarpContains(const std::string& lowerText, const std::string& lowerPattern) {
    // Atalho barato; também evita depender de como o Rabin-Karp trata
    // padrões maiores que o texto.
    if (normalizedPattern.size() > normalizedText.size()) {
        return false;
    }
    return RabinKarp::find_first(toKarpView(normalizedText), toKarpView(normalizedPattern))
        .has_value();
}

// 'title' e 'description' já normalizados ficam vazios quando o escopo exclui o campo correspondente.
bool matchesPatterns(const std::string& title, const std::string& description,
                     const std::vector<std::string>& normalizedPatterns,
                     PatternMatchMode mode, bool inTitle, bool inDescription) {
    const auto matches = [&](const std::string& pattern) {
        return (inTitle && rabinKarpContains(title, pattern)) ||
               (inDescription && rabinKarpContains(description, pattern));
    };

    return mode == PatternMatchMode::All
               ? std::all_of(normalizedPatterns.begin(), normalizedPatterns.end(), matches)
               : std::any_of(normalizedPatterns.begin(), normalizedPatterns.end(), matches);
}



void requireNonEmptyTerm(const std::string& term, const char* operation) {
    if (term.empty()) {
        throw std::invalid_argument(std::string("SearchService::") + operation +
                                    ": search term must not be empty");
    }
}

// Bytes >= 0x80 fazem parte de sequências UTF-8; tratá-los como letras evita
// partir palavras não-latinas no meio.
bool isWordByte(unsigned char c) {
    return c >= 0x80 || std::isalnum(c) != 0;
}

// O texto já chega normalizado (minúsculas, sem acentos), então só separa.
void appendWords(const std::string& normalizedText, std::vector<std::string>& words) {
    std::string current;
    for (unsigned char c : normalizedText) {
        if (isWordByte(c)) {
            current.push_back(static_cast<char>(c));
        } else if (!current.empty()) {
            words.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) {
        words.push_back(std::move(current));
    }
}

// Único ponto que conhece a conversão std::string -> LevenView.
LevenView toLevenView(const std::string& text) {
    return LevenView(text.data(), text.size());
}

// Busca textual: o repositório não tem índice para texto, então varre findAll().
template <typename Predicate>
std::vector<Task> filterTasks(const SearchService::Repository& repository, Predicate predicate) {
    std::vector<Task> result;
    for (Task& task : repository.findAll()) {
        if (predicate(task)) {
            result.push_back(std::move(task));
        }
    }
    return result;
}

// Bytes >= 0x80 fazem parte de sequências UTF-8; tratá-los como letras evita
// partir palavras acentuadas no meio.
bool isWordByte(unsigned char c) {
    return c >= 0x80 || std::isalnum(c) != 0;
}

void appendWords(const std::string& text, std::vector<std::string>& words) {
    std::string current;
    for (unsigned char c : text) {
        if (isWordByte(c)) {
            current.push_back(c < 0x80 ? static_cast<char>(std::tolower(c))
                                       : static_cast<char>(c));
        } else if (!current.empty()) {
            words.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) {
        words.push_back(std::move(current));
    }
}

std::vector<std::string> extractIndexWords(const Task& task) {
    std::vector<std::string> words;
    appendWords(task.title(), words);
    appendWords(task.description(), words);
    std::sort(words.begin(), words.end());
    words.erase(std::unique(words.begin(), words.end()), words.end());
    return words;
}

// Único ponto que conhece a conversão std::string -> LevenView.
LevenView toLevenView(const std::string& text) {
    return LevenView(text.data(), text.size());
}

struct Candidate {
    std::string word;
    std::size_t distance;
    bool isCompletion;
};

}  // namespace

int compareIgnoreCase(const std::string& a, const std::string& b) {
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const int ca = std::tolower(static_cast<unsigned char>(a[i]));
        const int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) {
            return ca < cb ? -1 : 1;
        }
    }
    if (a.size() == b.size()) {
        return 0;
    }
    return a.size() < b.size() ? -1 : 1;
}

int compareByField(const Task& a, const Task& b, SortField field) {
    switch (field) {
        case SortField::Title:
            return compareIgnoreCase(a.title(), b.title());
        case SortField::Priority:
            return static_cast<int>(a.priority()) - static_cast<int>(b.priority());
        case SortField::Status:
            return static_cast<int>(a.status()) - static_cast<int>(b.status());
        case SortField::Deadline:
            break;  // tratado em sortTasks
    }
    return 0;
}

SearchService::SearchService(const Repository& repository,const TextNormalizer& normalizer)
    : repository_(repository),normalizer_(normalizer) {
    rebuildIndex();
}

std::string SearchService::normalize(const std::string& text) const {
    const TextNormalizer::NormalizerBuffer buffer =
        normalizer_.normalize(TextNormalizer::NormalizerView(text.data(), text.size()));
    const TextNormalizer::NormalizerView normalized = buffer.view();
    return std::string(normalized.data(), normalized.size());
}

std::string SearchService::normalizeTerm(const std::string& term, const char* operation) const {
    requireNonEmptyTerm(term, operation);
    std::string normalized = normalize(term);
    // Ex.: um termo composto só de acentos soltos some na normalização;
    // buscá-lo casaria com tudo (ou com nada) de forma enganosa.
    if (normalized.empty()) {
        throw std::invalid_argument(std::string("SearchService::") + operation +
                                    ": search term has no searchable content");
    }
    return normalized;
}

std::vector<std::string> SearchService::extractIndexWords(const Task& task) const {
    std::vector<std::string> words;
    appendWords(normalize(task.title()), words);
    appendWords(normalize(task.description()), words);
    std::sort(words.begin(), words.end());
    words.erase(std::unique(words.begin(), words.end()), words.end());
    return words;
}

std::optional<Task> SearchService::searchById(const TaskId& id) const {
    return repository_.findById(id);
}

std::vector<Task> SearchService::searchByTitle(const std::string& term) const {
    const std::string key = normalizeTerm(term, "searchByTitle");
    return filterTasks(repository_, [this, &key](const Task& task) {
        return normalize(task.title()).find(key) != std::string::npos;
    });
}

std::vector<Task> SearchService::searchByDescription(const std::string& term) const {
    const std::string key = normalizeTerm(term, "searchByDescription");
    return filterTasks(repository_, [this, &key](const Task& task) {
        return normalize(task.description()).find(key) != std::string::npos;
    });
}

std::vector<Task> SearchService::searchBySubstring(const std::string& term,
                                                   SubstringScope scope,
                                                   SortSpec spec) const {
    const std::string key = normalizeTerm(term, "searchBySubstring");

    const bool inTitle = scope != SubstringScope::Description;
    const bool inDescription = scope != SubstringScope::Title;

    std::vector<Task> result = filterTasks(
        repository_, [this, &key, inTitle, inDescription](const Task& task) {
            return (inTitle && kmpContains(normalize(task.title()), key)) ||
                   (inDescription && kmpContains(normalize(task.description()), key));
        });

    sortTasks(result, spec);
    return result;
}

std::vector<Task> SearchService::searchByPattern(const std::string& pattern,
                                                 SubstringScope scope,
                                                 SortSpec spec) const {
        requireNonEmptyTerm(pattern, "searchByPattern");
    return searchByPatterns({pattern}, PatternMatchMode::Any, scope, spec);
}

std::vector<Task> SearchService::searchByPatterns(const std::vector<std::string>& patterns,
                                                  PatternMatchMode mode,
                                                  SubstringScope scope,
                                                  SortSpec spec) const {
     if (patterns.empty()) {
        throw std::invalid_argument(
            "SearchService::searchByPatterns: at least one pattern is required");
    }

    std::vector<std::string> normalizedPatterns;
    normalizedPatterns.reserve(patterns.size());
    for (const std::string& pattern : patterns) {
        normalizedPatterns.push_back(normalizeTerm(pattern, "searchByPatterns"));
    }

    const bool inTitle = scope != SubstringScope::Description;
    const bool inDescription = scope != SubstringScope::Title;

    std::vector<Task> result = filterTasks(
        repository_, [this, &normalizedPatterns, mode, inTitle, inDescription](const Task& task) {
            // Normaliza cada campo uma única vez por tarefa, não por padrão.
            const std::string title = inTitle ? normalize(task.title()) : std::string();
            const std::string description =
                inDescription ? normalize(task.description()) : std::string();
            return matchesPatterns(title, description, normalizedPatterns, mode, inTitle,
                                   inDescription);
        });

    sortTasks(result, spec);
    return result;
}

std::vector<Task> SearchService::searchByStatus(Status status) const {
    return repository_.findByStatus(status);
}

std::vector<Task> SearchService::searchByPriority(Priority priority) const {
    return repository_.findByPriority(priority);
}

std::vector<Task> SearchService::searchByDeadline(TimePoint from, TimePoint to) const {
    if (from > to) {
        throw std::invalid_argument(
            "SearchService::searchByDeadline: 'from' must not be later than 'to'");
    }

    // O repositório usa [from, to); o serviço expõe [from, to]. Para incluir
    // 'to', avança um tick no limite superior.
    if (to < TimePoint::max()) {
        return repository_.findByDeadlineBetween(from, to + TimePoint::duration{1});
    }

    // Caso extremo (to == TimePoint::max()): não há tick seguinte. Consulta
    // [from, max) e acrescenta as tarefas com deadline exatamente em max,
    // mantendo a ordenação por deadline.
    std::vector<Task> result = repository_.findByDeadlineBetween(from, to);
    for (Task& task : repository_.findAll()) {
        if (task.deadline().has_value() && *task.deadline() == to) {
            result.push_back(std::move(task));
        }
    }
    return result;
}

std::vector<std::string> SearchService::autocomplete(const std::string& prefix,
                                                     std::size_t maxResults) const {
    return index_.autocomplete(normalizeTerm(prefix, "autocomplete"), maxResults);
}

std::vector<std::string> SearchService::suggest(const std::string& term,
                                                std::size_t maxResults,
                                                double minSimilarity) const {
    const std::string normalizedTerm = normalizeTerm(term, "suggest");
    if (minSimilarity < 0.0 || minSimilarity > 1.0) {
        throw std::invalid_argument(
            "SearchService::suggest: minSimilarity must be in [0, 1]");
    }

    const std::string lowerTerm = toLower(term);
    const LevenView termView = toLevenView(lowerTerm);

    std::vector<Candidate> candidates;
    for (std::string& word : index_.autocomplete("")) {
        if (word == lowerTerm) {
            continue;  // não sugere o que o usuário já digitou
        }

        const bool isCompletion =
            word.size() > lowerTerm.size() &&
            word.compare(0, lowerTerm.size(), lowerTerm) == 0;

        // Poda barata: a diferença de tamanho é um limite inferior da
        // distância, então evita montar a matriz quando já não há chance.
        const std::size_t maxLen = std::max(word.size(), lowerTerm.size());
        const std::size_t lenDiff = word.size() > lowerTerm.size()
                                        ? word.size() - lowerTerm.size()
                                        : lowerTerm.size() - word.size();
        if (!isCompletion &&
            1.0 - static_cast<double>(lenDiff) / static_cast<double>(maxLen) < minSimilarity) {
            continue;
        }

        const LevenView wordView = toLevenView(word);
        const std::size_t distance = Levenshtein::levenshtein_distance(termView, wordView);
        const double similarity = Levenshtein::levenshtein_similarity(termView, wordView);

        if (isCompletion || similarity >= minSimilarity) {
            candidates.push_back({std::move(word), distance, isCompletion});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  if (a.isCompletion != b.isCompletion) return a.isCompletion;
                  if (a.distance != b.distance) return a.distance < b.distance;
                  return a.word < b.word;
              });

    if (maxResults > 0 && candidates.size() > maxResults) {
        candidates.resize(maxResults);
    }

    std::vector<std::string> result;
    result.reserve(candidates.size());
    for (Candidate& candidate : candidates) {
        result.push_back(std::move(candidate.word));
    }
    return result;
}

void SearchService::indexTask(const Task& task) {
    index_.updateTask(task.id(), extractIndexWords(task));
}

void SearchService::removeTaskFromIndex(const TaskId& id) {
    index_.removeTask(id);
}

void SearchService::rebuildIndex() {
    // Constrói em uma Trie nova e troca no final: se algo lançar exceção,
    // o índice anterior permanece intacto.
    Trie fresh;
    for (const Task& task : repository_.findAll()) {
        fresh.updateTask(task.id(), extractIndexWords(task));
    }
    index_ = std::move(fresh);
}

void SearchService::sortTasks(std::vector<Task>& tasks, SortSpec spec) {
    const bool asc = spec.order == SortOrder::Ascending;

    if (spec.field == SortField::Deadline) {
        std::stable_sort(tasks.begin(), tasks.end(), [asc](const Task& a, const Task& b) {
            const auto da = a.deadline();
            const auto db = b.deadline();
            if (!da.has_value() || !db.has_value()) {
                return da.has_value() && !db.has_value();  // sem prazo vai para o fim
            }
            return asc ? *da < *db : *da > *db;
        });
        return;
    }

    std::stable_sort(tasks.begin(), tasks.end(), [asc, spec](const Task& a, const Task& b) {
        const int cmp = compareByField(a, b, spec.field);
        return asc ? cmp < 0 : cmp > 0;
    });
}

std::vector<Task> SearchService::searchAll(SortSpec spec) const {
    std::vector<Task> result = repository_.findAll();
    sortTasks(result, spec);
    return result;
}

std::vector<Task> SearchService::searchByPriorityRange(Priority from, Priority to,
                                                       SortSpec spec) const {
    const int lo = static_cast<int>(from);
    const int hi = static_cast<int>(to);
    if (lo > hi) {
        throw std::invalid_argument(
            "SearchService::searchByPriorityRange: 'from' must not be greater than 'to'");
    }

    std::vector<Task> result = filterTasks(repository_, [lo, hi](const Task& task) {
        const int p = static_cast<int>(task.priority());
        return p >= lo && p <= hi;
    });
    sortTasks(result, spec);
    return result;                                                       
}

std::vector<Task> SearchService::searchByTitleRange(const std::string& from,
                                                    const std::string& to,
                                                    SortSpec spec) const {
    // Limites vazios são válidos aqui (normalize("") == ""), como antes.
    const std::string lowerBound = normalize(from);
    const std::string upperBound = normalize(to);
    if (lowerBound > upperBound) {
        throw std::invalid_argument(
            "SearchService::searchByTitleRange: 'from' must not be greater than 'to'");
    }

    // A forma normalizada é ASCII minúsculo para texto latino, então a
    // comparação byte a byte de std::string respeita a ordem lexicográfica.
    std::vector<Task> result =
        filterTasks(repository_, [this, &lowerBound, &upperBound](const Task& task) {
            const std::string title = normalize(task.title());
            return title >= lowerBound && title <= upperBound;
        });
    sortTasks(result, spec);
    return result;
}

} // namespace SearchService