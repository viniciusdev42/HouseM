#include "SearchService.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace SearchService {

namespace {

using IdSet = std::set<TaskId>;

// ---------- Conversões de view (únicos pontos que conhecem cada tipo) ----------
// Se o construtor de alguma StringView for diferente, só estas funções mudam.

KView toView(const std::string& text) {
    return KView(text.data(), text.size());
}

KarpView toKarpView(const std::string& text) {
    return KarpView(text.data(), text.size());
}

LevenView toLevenView(const std::string& text) {
    return LevenView(text.data(), text.size());
}

TokenizerView toTokenizerView(const std::string& text) {
    return TokenizerView(text.data(), text.size());
}

// ---------- Casamento de texto ----------

// Ambos os argumentos já devem estar normalizados (o chamador normaliza uma
// única vez por tarefa, e não uma vez por padrão).
bool kmpContains(const std::string& normalizedText, const std::string& normalizedTerm) {
    return KMP::search(toView(normalizedText), toView(normalizedTerm)).size() > 0;
}

// Ambos os argumentos já devem estar normalizados.
bool rabinKarpContains(const std::string& normalizedText, const std::string& normalizedPattern) {
    // Atalho barato; também evita depender de como o Rabin-Karp trata
    // padrões maiores que o texto.
    if (normalizedPattern.size() > normalizedText.size()) {
        return false;
    }
    return RabinKarp::find_first(toKarpView(normalizedText), toKarpView(normalizedPattern))
        .has_value();
}

// 'title' e 'description' já normalizados ficam vazios quando o escopo exclui
// o campo correspondente.
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

// ---------- Validação ----------

void requireNonEmptyTerm(const std::string& term, const char* operation) {
    if (term.empty()) {
        throw std::invalid_argument(std::string("SearchService::") + operation +
                                    ": search term must not be empty");
    }
}

[[noreturn]] void throwNoSearchableContent(const char* operation) {
    throw std::invalid_argument(std::string("SearchService::") + operation +
                                ": search term has no searchable content");
}

bool endsWithSpace(const std::string& text) {
    return !text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0;
}

// ---------- Tokenizer ----------

// Configuração usada tanto na indexação quanto nas consultas: os dois lados
// PRECISAM tokenizar igual, senão o índice não casa com a busca.
// Pontuação é descartada; hífen/apóstrofo entre letras ficam no token.
::Tokenizer::Text::TokenizerOptions indexTokenizerOptions() {
    ::Tokenizer::Text::TokenizerOptions options;
    options.punctuation = ::Tokenizer::Text::PunctuationMode::Discard;
    options.removeEmptyTokens = true;
    options.keepIntraWordConnectors = true;
    return options;
}

// Adiciona cada token e, se for composto (hífen/apóstrofo), também suas
// partes, para que "chuva" encontre "guarda-chuva".
void appendIndexWords(const std::vector<std::string>& tokens, std::vector<std::string>& words) {
    for (const std::string& token : tokens) {
        words.push_back(token);

        std::vector<std::string> parts;
        std::string part;
        for (const char c : token) {
            if (c == '-' || c == '\'') {
                if (!part.empty()) {
                    parts.push_back(std::move(part));
                    part.clear();
                }
            } else {
                part.push_back(c);
            }
        }
        if (!part.empty()) {
            parts.push_back(std::move(part));
        }

        if (parts.size() > 1 || (parts.size() == 1 && parts.front() != token)) {
            for (std::string& p : parts) {
                words.push_back(std::move(p));
            }
        }
    }
}

// ---------- Conjuntos de ids ----------

IdSet intersect(const IdSet& a, const IdSet& b) {
    IdSet out;
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                          std::inserter(out, out.begin()));
    return out;
}

IdSet combineIds(const std::vector<IdSet>& sets, PatternMatchMode mode) {
    if (sets.empty()) {
        return {};
    }
    IdSet result = sets.front();
    for (std::size_t i = 1; i < sets.size(); ++i) {
        if (mode == PatternMatchMode::All) {
            result = intersect(result, sets[i]);
            if (result.empty()) {
                break;
            }
        } else {
            result.insert(sets[i].begin(), sets[i].end());
        }
    }
    return result;
}

// ---------- Varredura e ordenação ----------

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

struct Candidate {
    std::string word;
    std::size_t distance;
    bool isCompletion;
};

}  // namespace

// ============================== Construção ==============================

SearchService::SearchService(const Repository& repository, const TextNormalizer& normalizer)
    : repository_(repository),
      normalizer_(normalizer),
      tokenizer_(indexTokenizerOptions()) {
    rebuildIndex();
}

// ============================ Texto / Tokenizer ============================

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
        throwNoSearchableContent(operation);
    }
    return normalized;
}

// A normalização vem ANTES da tokenização: o Tokenizer só enxerga texto já
// em minúsculas e sem acentos, igual ao que foi indexado.
std::vector<std::string> SearchService::tokenize(const std::string& text) const {
    const std::string normalized = normalize(text);
    return tokenizer_.tokenize(toTokenizerView(normalized));
}

std::vector<std::string> SearchService::queryTokens(const std::string& query,
                                                    const char* operation) const {
    requireNonEmptyTerm(query, operation);
    std::vector<std::string> tokens = tokenize(query);
    if (tokens.empty()) {
        throwNoSearchableContent(operation);
    }
    std::sort(tokens.begin(), tokens.end());
    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
    return tokens;
}

std::string SearchService::activeToken(const std::string& text, const char* operation) const {
    requireNonEmptyTerm(text, operation);
    std::vector<std::string> tokens = tokenize(text);
    if (tokens.empty()) {
        throwNoSearchableContent(operation);
    }
    return std::move(tokens.back());
}

std::vector<std::string> SearchService::extractIndexWords(const Task& task) const {
    std::vector<std::string> words;
    appendIndexWords(tokenize(task.title()), words);
    appendIndexWords(tokenize(task.description()), words);
    std::sort(words.begin(), words.end());
    words.erase(std::unique(words.begin(), words.end()), words.end());
    return words;
}

// ============================ Buscas simples ============================

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

// ======================= Substring / padrões (KMP, RK) =======================

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

// ===================== Termos / prefixos (índice invertido) =====================

SearchService::IdSet SearchService::idsForToken(const std::string& token) const {
    const auto it = postings_.find(token);
    return it == postings_.end() ? IdSet{} : it->second;
}

SearchService::IdSet SearchService::idsForPrefix(const std::string& prefix) const {
    // O próprio prefixo também é consultado: a Trie pode ou não devolver a
    // palavra idêntica ao prefixo, e assim o resultado não depende disso.
    IdSet ids = idsForToken(prefix);
    for (const std::string& word : index_.autocomplete(prefix, 0)) {
        const IdSet wordIds = idsForToken(word);
        ids.insert(wordIds.begin(), wordIds.end());
    }
    return ids;
}

std::vector<Task> SearchService::tasksWithIds(const IdSet& ids, SortSpec spec) const {
    if (ids.empty()) {
        return {};
    }
    std::vector<Task> result = filterTasks(
        repository_, [&ids](const Task& task) { return ids.count(task.id()) > 0; });
    sortTasks(result, spec);
    return result;
}

std::vector<Task> SearchService::searchByTerms(const std::string& query,
                                               PatternMatchMode mode,
                                               SortSpec spec) const {
    std::vector<IdSet> perToken;
    for (const std::string& token : queryTokens(query, "searchByTerms")) {
        perToken.push_back(idsForToken(token));
    }
    return tasksWithIds(combineIds(perToken, mode), spec);
}

std::vector<Task> SearchService::searchByPrefix(const std::string& query,
                                                PatternMatchMode mode,
                                                SortSpec spec) const {
    std::vector<IdSet> perPrefix;
    for (const std::string& prefix : queryTokens(query, "searchByPrefix")) {
        perPrefix.push_back(idsForPrefix(prefix));
    }
    return tasksWithIds(combineIds(perPrefix, mode), spec);
}

// ========================= Autocomplete / sugestões =========================

std::vector<std::string> SearchService::autocomplete(const std::string& prefix,
                                                     std::size_t maxResults) const {
    const std::string token = activeToken(prefix, "autocomplete");
    // "reunião " (com espaço) já terminou a palavra: nada a completar.
    if (endsWithSpace(prefix)) {
        return {};
    }
    return index_.autocomplete(token, maxResults);
}

std::vector<std::string> SearchService::suggest(const std::string& term,
                                                std::size_t maxResults,
                                                double minSimilarity) const {
    const std::string normalizedTerm = activeToken(term, "suggest");
    if (minSimilarity < 0.0 || minSimilarity > 1.0) {
        throw std::invalid_argument(
            "SearchService::suggest: minSimilarity must be in [0, 1]");
    }

    const LevenView termView = toLevenView(normalizedTerm);

    std::vector<Candidate> candidates;
    for (std::string& word : index_.autocomplete("")) {
        if (word == normalizedTerm) {
            continue;  // não sugere o que o usuário já digitou
        }

        const bool isCompletion =
            word.size() > normalizedTerm.size() &&
            word.compare(0, normalizedTerm.size(), normalizedTerm) == 0;

        // Poda barata: a diferença de tamanho é um limite inferior da
        // distância, então evita montar a matriz quando já não há chance.
        const std::size_t maxLen = std::max(word.size(), normalizedTerm.size());
        const std::size_t lenDiff = word.size() > normalizedTerm.size()
                                        ? word.size() - normalizedTerm.size()
                                        : normalizedTerm.size() - word.size();
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

// ================================ Índice ================================

void SearchService::addToPostings(const TaskId& id, const std::vector<std::string>& words) {
    for (const std::string& word : words) {
        postings_[word].insert(id);
    }
}

// Varre o vocabulário inteiro (O(V)): evita guardar um segundo mapa
// tarefa -> palavras só para remoção.
void SearchService::removeFromPostings(const TaskId& id) {
    for (auto it = postings_.begin(); it != postings_.end();) {
        it->second.erase(id);
        it = it->second.empty() ? postings_.erase(it) : std::next(it);
    }
}

void SearchService::indexTask(const Task& task) {
    const std::vector<std::string> words = extractIndexWords(task);
    index_.updateTask(task.id(), words);
    removeFromPostings(task.id());  // reindexar substitui as palavras antigas
    addToPostings(task.id(), words);
}

void SearchService::removeTaskFromIndex(const TaskId& id) {
    index_.removeTask(id);
    removeFromPostings(id);
}

void SearchService::rebuildIndex() {
    // Constrói tudo em estruturas novas e troca no final: se algo lançar
    // exceção, o índice anterior permanece intacto.
    Trie fresh;
    std::map<std::string, IdSet> freshPostings;
    for (const Task& task : repository_.findAll()) {
        const std::vector<std::string> words = extractIndexWords(task);
        fresh.updateTask(task.id(), words);
        for (const std::string& word : words) {
            freshPostings[word].insert(task.id());
        }
    }
    index_ = std::move(fresh);
    postings_ = std::move(freshPostings);
}

// ================================ Ordenação ================================

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

}  // namespace SearchService
