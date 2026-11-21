#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/Main/Model/Services/Search/SearchService.hpp"

namespace {

using Repo = ITaskRepository::InMemoryTaskRepository;
using Clock = ITaskRepository::ITaskRepository::Clock;
using TimePoint = ITaskRepository::ITaskRepository::TimePoint;
using ::Task::Priority;
using ::Task::Status;
using TaskT = ::Task::Task;
using TaskIdT = ::Task::TaskId;
using SearchService::PatternMatchMode;
using SearchService::SortField;
using SearchService::SortOrder;
using SearchService::SortSpec;
using SearchService::SubstringScope;

// ============================================================================
// ADAPTAÇÃO: Task.hpp não foi enviado. Se o construtor/os enumeradores de
// Task forem diferentes, ajuste SOMENTE este bloco.
// ============================================================================
constexpr Priority kLow = Priority::Low;
constexpr Priority kHigh = Priority::High;
constexpr Priority kCritical = Priority::Critical;
constexpr Status kPending = Status::Pending;
constexpr Status kCompleted = Status::Completed;

TaskT makeTask(const std::string& id, const std::string& title, const std::string& description,
               Status status, Priority priority, std::optional<TimePoint> deadline = std::nullopt) {
    TaskT task(TaskIdT(id), title, description, priority, deadline);
    task.setStatus(status);
    return task;
}
// ============================================================================

// ---------- Mini framework ----------

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        ++g_checks;                                                                  \
        if (!(cond)) {                                                               \
            ++g_failures;                                                            \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): " #cond << "\n";      \
        }                                                                            \
    } while (0)

#define CHECK_THROWS_INVALID(expr)                                                   \
    do {                                                                             \
        ++g_checks;                                                                  \
        try {                                                                        \
            (void)(expr);                                                            \
            ++g_failures;                                                            \
            std::cerr << "  FALHOU (linha " << __LINE__                              \
                      << "): esperava std::invalid_argument: " #expr "\n";           \
        } catch (const std::invalid_argument&) {                                     \
        } catch (...) {                                                              \
            ++g_failures;                                                            \
            std::cerr << "  FALHOU (linha " << __LINE__ << "): excecao de tipo errado\n"; \
        }                                                                            \
    } while (0)

struct TestCase {
    std::string name;
    std::function<void()> body;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

#define TEST(name)                                       \
    void name();                                         \
    const Registrar registrar_##name(#name, name);       \
    void name()

// ---------- Fixture ----------

const TimePoint kBase = Clock::now();
TimePoint day(int n) { return kBase + std::chrono::hours(24 * n); }

// t1: reunião (título) | t4: reunião (descrição) | t2: composto "guarda-chuva"
// t3: concluída, prioridade crítica
void populate(Repo& repo) {
    repo.add(makeTask("t1", "Reunião com João", "Discutir o projeto", kPending, kHigh, day(1)));
    repo.add(makeTask("t2", "Comprar guarda-chuva", "Loja do centro", kPending, kLow));
    repo.add(makeTask("t3", "Estudar C++", "Revisar algoritmos de busca", kCompleted, kCritical, day(3)));
    repo.add(makeTask("t4", "Relatório mensal", "Enviar reunião final", kPending, kHigh, day(2)));
}

std::vector<std::string> ids(const std::vector<TaskT>& tasks) {
    std::vector<std::string> out;
    for (const TaskT& t : tasks) out.push_back(t.id().value());
    return out;
}

// Compara sem considerar a ordem.
bool sameIds(const std::vector<TaskT>& tasks, std::vector<std::string> expected) {
    std::vector<std::string> actual = ids(tasks);
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    return actual == expected;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

struct Env {
    Repo repo;
    Normalizer::Normalizer normalizer;
    SearchService::SearchService service;
    Env() : service((populate(repo), repo), normalizer) {}
};

// ---------- Buscas simples ----------

TEST(searchById_encontraEnaoEncontra) {
    Env e;
    auto found = e.service.searchById(TaskIdT("t1"));
    CHECK(found.has_value() && found->title() == "Reunião com João");
    CHECK(!e.service.searchById(TaskIdT("inexistente")).has_value());
}

TEST(searchByTitle_ignoraCaixaEacento) {
    Env e;
    CHECK(sameIds(e.service.searchByTitle("JOAO"), {"t1"}));
    CHECK(e.service.searchByTitle("zzz").empty());
    CHECK_THROWS_INVALID(e.service.searchByTitle(""));
}

TEST(searchByDescription_ignoraCaixaEacento) {
    Env e;
    CHECK(sameIds(e.service.searchByDescription("REUNIAO"), {"t4"}));
    CHECK_THROWS_INVALID(e.service.searchByDescription(""));
}

TEST(searchByStatus_ePriority) {
    Env e;
    CHECK(sameIds(e.service.searchByStatus(kCompleted), {"t3"}));
    CHECK(sameIds(e.service.searchByStatus(kPending), {"t1", "t2", "t4"}));
    CHECK(sameIds(e.service.searchByPriority(kHigh), {"t1", "t4"}));
}

TEST(searchByDeadline_intervaloFechado) {
    Env e;
    // O limite superior (day(2)) deve ser incluído.
    CHECK(sameIds(e.service.searchByDeadline(day(1), day(2)), {"t1", "t4"}));
    CHECK(sameIds(e.service.searchByDeadline(day(3), day(3)), {"t3"}));
    CHECK_THROWS_INVALID(e.service.searchByDeadline(day(2), day(1)));
}

TEST(searchByPriorityRange_filtraEordena) {
    Env e;
    auto result = e.service.searchByPriorityRange(kLow, kHigh);
    CHECK(sameIds(result, {"t1", "t2", "t4"}));
    CHECK(result.front().id().value() == "t2");  // prioridade crescente: Low primeiro
    CHECK_THROWS_INVALID(e.service.searchByPriorityRange(kHigh, kLow));
}

TEST(searchByTitleRange_intervaloLexicografico) {
    Env e;
    // "comprar guarda-chuva" e "estudar c++" estão entre "c" e "f".
    CHECK(ids(e.service.searchByTitleRange("c", "f")) == (std::vector<std::string>{"t2", "t3"}));
    CHECK_THROWS_INVALID(e.service.searchByTitleRange("z", "a"));
}

// ---------- Substring / padrões ----------

TEST(searchBySubstring_respeitaEscopo) {
    Env e;
    CHECK(sameIds(e.service.searchBySubstring("reuniao", SubstringScope::Title), {"t1"}));
    CHECK(sameIds(e.service.searchBySubstring("reuniao", SubstringScope::Description), {"t4"}));
    CHECK(sameIds(e.service.searchBySubstring("REUNIAO", SubstringScope::Both), {"t1", "t4"}));
    CHECK_THROWS_INVALID(e.service.searchBySubstring(""));
}

TEST(searchByPattern_RabinKarp) {
    Env e;
    CHECK(sameIds(e.service.searchByPattern("guarda"), {"t2"}));
    CHECK(e.service.searchByPattern("padrao-bem-maior-que-qualquer-texto-existente").empty());
    CHECK_THROWS_INVALID(e.service.searchByPattern(""));
}

TEST(searchByPatterns_AnyEAll) {
    Env e;
    const std::vector<std::string> patterns = {"reuniao", "projeto"};
    CHECK(sameIds(e.service.searchByPatterns(patterns, PatternMatchMode::Any), {"t1", "t4"}));
    CHECK(sameIds(e.service.searchByPatterns(patterns, PatternMatchMode::All), {"t1"}));
    CHECK_THROWS_INVALID(e.service.searchByPatterns({}));
    CHECK_THROWS_INVALID(e.service.searchByPatterns({"ok", ""}));
}

// ---------- Índice invertido ----------

TEST(searchByTerms_AllAnyEComposto) {
    Env e;
    CHECK(sameIds(e.service.searchByTerms("projeto reuniao"), {"t1"}));  // padrão: All
    CHECK(sameIds(e.service.searchByTerms("projeto reuniao", PatternMatchMode::Any), {"t1", "t4"}));
    CHECK(sameIds(e.service.searchByTerms("chuva"), {"t2"}));  // parte de "guarda-chuva"
    CHECK(e.service.searchByTerms("inexistente").empty());
    CHECK_THROWS_INVALID(e.service.searchByTerms(""));
    CHECK_THROWS_INVALID(e.service.searchByTerms("!!!"));  // sem conteúdo pesquisável
}

TEST(searchByPrefix_expandePrefixos) {
    Env e;
    CHECK(sameIds(e.service.searchByPrefix("reun jo"), {"t1"}));
    CHECK(sameIds(e.service.searchByPrefix("reun", PatternMatchMode::Any), {"t1", "t4"}));
    CHECK_THROWS_INVALID(e.service.searchByPrefix(""));
}

// ---------- Ordenação ----------

TEST(searchAll_ePorPrioridadeDecrescente) {
    Env e;
    CHECK(e.service.searchAll().size() == 4);
    auto byPriority = e.service.searchAll({SortField::Priority, SortOrder::Descending});
    CHECK(byPriority.front().id().value() == "t3");
    CHECK(byPriority.back().id().value() == "t2");
}

TEST(sortTasks_deadlineSemPrazoSempreNoFim) {
    Env e;
    CHECK(ids(e.service.searchAll({SortField::Deadline, SortOrder::Ascending})) ==
          (std::vector<std::string>{"t1", "t4", "t3", "t2"}));
    CHECK(ids(e.service.searchAll({SortField::Deadline, SortOrder::Descending})) ==
          (std::vector<std::string>{"t3", "t4", "t1", "t2"}));
}

// ---------- Autocomplete / sugestões ----------

TEST(autocomplete_completaUltimoToken) {
    Env e;
    CHECK(contains(e.service.autocomplete("reu"), "reuniao"));
    CHECK(contains(e.service.autocomplete("reuniao com jo"), "joao"));
    CHECK(e.service.autocomplete("reuniao ").empty());  // termina em espaço
    CHECK(e.service.autocomplete("re", 1).size() == 1);  // respeita o limite
    CHECK_THROWS_INVALID(e.service.autocomplete(""));
}

TEST(suggest_corrigeCompletaENaoSugereOProprioTermo) {
    Env e;
    CHECK(contains(e.service.suggest("reunioa"), "reuniao"));  // erro de digitação
    CHECK(contains(e.service.suggest("reu"), "reuniao"));      // completamento
    CHECK(!contains(e.service.suggest("reuniao"), "reuniao"));
    CHECK(e.service.suggest("re", 1).size() <= 1);
    CHECK_THROWS_INVALID(e.service.suggest(""));
    CHECK_THROWS_INVALID(e.service.suggest("reuniao", 5, 1.5));
    CHECK_THROWS_INVALID(e.service.suggest("reuniao", 5, -0.1));
}

// ---------- Manutenção do índice ----------

TEST(indice_indexRemoveERebuild) {
    Env e;
    CHECK(e.service.searchByTerms("dentista").empty());

    // O índice só é atualizado quando o serviço é avisado.
    e.repo.add(makeTask("t5", "Dentista", "Consulta anual", kPending, kLow));
    CHECK(e.service.searchByTerms("dentista").empty());

    e.service.indexTask(*e.repo.findById(TaskIdT("t5")));
    CHECK(sameIds(e.service.searchByTerms("dentista"), {"t5"}));

    e.service.removeTaskFromIndex(TaskIdT("t5"));
    CHECK(e.service.searchByTerms("dentista").empty());

    e.service.rebuildIndex();  // t5 ainda está no repositório
    CHECK(sameIds(e.service.searchByTerms("dentista"), {"t5"}));
}

}  // namespace

int main() {
    int failedTests = 0;
    for (const TestCase& test : registry()) {
        const int before = g_failures;
        try {
            test.body();
        } catch (const std::exception& ex) {
            ++g_failures;
            std::cerr << "  EXCECAO inesperada: " << ex.what() << "\n";
        }
        const bool ok = g_failures == before;
        failedTests += ok ? 0 : 1;
        std::cout << (ok ? "[ OK ] " : "[FAIL] ") << test.name << "\n";
    }
    std::cout << "\n" << registry().size() - failedTests << "/" << registry().size()
              << " testes passaram (" << g_checks << " verificacoes)\n";
    return g_failures == 0 ? 0 : 1;
}
