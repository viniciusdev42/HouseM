#include "HashTable.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

// ---------------------------------------------------------------------------
// Construção, destruição, cópia e movimentação
// ---------------------------------------------------------------------------

HashTable::HashTable(std::size_t initialCapacity)
    : buckets_(),
      size_(0),
      initialCapacity_(initialCapacity),
      maxLoadFactor_(kDefaultMaxLoadFactor) {
    if (initialCapacity == 0) {
        throw std::invalid_argument(
            "HashTable: a capacidade inicial deve ser maior que zero");
    }
    buckets_.assign(initialCapacity_, nullptr);
}

HashTable::~HashTable() {
    destroyAllNodes();
}

HashTable::HashTable(const HashTable& other)
    : buckets_(other.buckets_.size(), nullptr),
      size_(0),
      initialCapacity_(other.initialCapacity_),
      maxLoadFactor_(other.maxLoadFactor_) {
    try {
        copyNodesFrom(other);
    } catch (...) {
        // O destrutor não é chamado se o construtor lança; limpa manualmente.
        destroyAllNodes();
        throw;
    }
}

HashTable& HashTable::operator=(const HashTable& other) {
    if (this != &other) {
        HashTable copy(other);  // copy-and-swap: garantia forte de exceção
        swap(copy);
    }
    return *this;
}

HashTable::HashTable(HashTable&& other) noexcept
    : buckets_(std::move(other.buckets_)),
      size_(other.size_),
      initialCapacity_(other.initialCapacity_),
      maxLoadFactor_(other.maxLoadFactor_) {
    // Estado válido (porém vazio) para o objeto movido.
    other.buckets_.clear();
    other.size_ = 0;
}

HashTable& HashTable::operator=(HashTable&& other) noexcept {
    if (this != &other) {
        swap(other);
    }
    return *this;
}

void HashTable::swap(HashTable& other) noexcept {
    std::swap(buckets_, other.buckets_);
    std::swap(size_, other.size_);
    std::swap(initialCapacity_, other.initialCapacity_);
    std::swap(maxLoadFactor_, other.maxLoadFactor_);
}

// ---------------------------------------------------------------------------
// Operações principais
// ---------------------------------------------------------------------------

bool HashTable::insert(const Key& key, const Value& value) {
    if (Node* existing = findNode(key)) {
        existing->value = value;
        return false;
    }

    // Redimensiona antes de inserir para manter loadFactor <= maxLoadFactor.
    growIfNeeded(size_ + 1);

    const std::size_t index = indexFor(key);
    buckets_[index] = new Node{key, value, buckets_[index]};
    ++size_;
    return true;
}

std::optional<HashTable::Value> HashTable::search(const Key& key) const {
    if (const Node* node = findNode(key)) {
        return node->value;
    }
    return std::nullopt;
}

bool HashTable::contains(const Key& key) const {
    return findNode(key) != nullptr;
}

bool HashTable::remove(const Key& key) {
    if (buckets_.empty()) {
        return false;
    }

    // 'link' aponta para o ponteiro que referencia o nó atual, o que elimina
    // o caso especial de remoção da cabeça da lista.
    Node** link = &buckets_[indexFor(key)];
    while (*link != nullptr) {
        if ((*link)->key == key) {
            Node* removed = *link;
            *link = removed->next;
            delete removed;
            --size_;
            shrinkIfNeeded();
            return true;
        }
        link = &(*link)->next;
    }
    return false;
}

void HashTable::clear() {
    destroyAllNodes();
}

// ---------------------------------------------------------------------------
// Consultas de estado
// ---------------------------------------------------------------------------

std::size_t HashTable::size() const noexcept {
    return size_;
}

bool HashTable::empty() const noexcept {
    return size_ == 0;
}

std::size_t HashTable::bucketCount() const noexcept {
    return buckets_.size();
}

double HashTable::loadFactor() const noexcept {
    if (buckets_.empty()) {
        return 0.0;
    }
    return static_cast<double>(size_) / static_cast<double>(buckets_.size());
}

double HashTable::maxLoadFactor() const noexcept {
    return maxLoadFactor_;
}

// ---------------------------------------------------------------------------
// Fator de carga e redimensionamento
// ---------------------------------------------------------------------------

void HashTable::setMaxLoadFactor(double maxLoad) {
    if (!(maxLoad > 0.0) || !std::isfinite(maxLoad)) {
        throw std::invalid_argument(
            "HashTable: o fator de carga máximo deve ser positivo e finito");
    }
    maxLoadFactor_ = maxLoad;
    growIfNeeded(size_);
}

void HashTable::rehash(std::size_t newBucketCount) {
    // Número mínimo de buckets que respeita o fator de carga máximo.
    const auto required = static_cast<std::size_t>(
        std::ceil(static_cast<double>(size_) / maxLoadFactor_));
    newBucketCount = std::max({newBucketCount, required, std::size_t{1}});

    if (newBucketCount == buckets_.size()) {
        return;
    }

    // A alocação do novo vetor é a única operação que pode lançar exceção;
    // ela ocorre antes de qualquer modificação (garantia forte).
    std::vector<Node*> newBuckets(newBucketCount, nullptr);

    // Reutiliza os nós existentes: apenas religa os ponteiros.
    std::vector<Node*> oldBuckets;
    oldBuckets.swap(buckets_);
    buckets_.swap(newBuckets);

    for (Node* head : oldBuckets) {
        while (head != nullptr) {
            Node* next = head->next;
            const std::size_t index = indexFor(head->key);
            head->next = buckets_[index];
            buckets_[index] = head;
            head = next;
        }
    }
}

void HashTable::ensureBuckets() {
    // Necessário apenas para objetos que sofreram move.
    if (buckets_.empty()) {
        buckets_.assign(initialCapacity_, nullptr);
    }
}

void HashTable::growIfNeeded(std::size_t prospectiveSize) {
    ensureBuckets();

    std::size_t target = buckets_.size();
    while (static_cast<double>(prospectiveSize) >
           maxLoadFactor_ * static_cast<double>(target)) {
        target *= 2;
    }

    if (target != buckets_.size()) {
        rehash(target);
    }
}

void HashTable::shrinkIfNeeded() {
    // Limite inferior = maxLoadFactor / 4. Ao reduzir pela metade, o novo fator
    // de carga fica abaixo de maxLoadFactor / 2, evitando oscilação
    // (crescer e encolher repetidamente) em torno do mesmo limite.
    const std::size_t halved = buckets_.size() / 2;
    if (halved >= initialCapacity_ && loadFactor() < maxLoadFactor_ / 4.0) {
        rehash(halved);
    }
}

// ---------------------------------------------------------------------------
// Auxiliares internos
// ---------------------------------------------------------------------------

std::size_t HashTable::hashKey(const Key& key) noexcept {
    // FNV-1a de 64 bits: simples, rápida e com boa dispersão para strings.
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;

    std::uint64_t hash = kOffsetBasis;
    for (unsigned char c : key) {
        hash ^= c;
        hash *= kPrime;
    }
    return static_cast<std::size_t>(hash);
}

std::size_t HashTable::indexFor(const Key& key) const noexcept {
    return hashKey(key) % buckets_.size();
}

HashTable::Node* HashTable::findNode(const Key& key) const noexcept {
    if (buckets_.empty()) {
        return nullptr;
    }
    for (Node* node = buckets_[indexFor(key)]; node != nullptr;
         node = node->next) {
        if (node->key == key) {
            return node;
        }
    }
    return nullptr;
}

void HashTable::destroyAllNodes() noexcept {
    for (Node*& head : buckets_) {
        while (head != nullptr) {
            Node* next = head->next;
            delete head;
            head = next;
        }
    }
    size_ = 0;
}

void HashTable::copyNodesFrom(const HashTable& other) {
    // Preserva a posição de cada bucket e a ordem das cadeias.
    for (std::size_t i = 0; i < other.buckets_.size(); ++i) {
        Node** tail = &buckets_[i];
        for (const Node* src = other.buckets_[i]; src != nullptr;
             src = src->next) {
            *tail = new Node{src->key, src->value, nullptr};
            tail = &(*tail)->next;
            ++size_;
        }
    }
}
