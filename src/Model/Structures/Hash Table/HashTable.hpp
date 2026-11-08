#ifndef HASH_TABLE
#define HASH_TABLE

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/**
 * Tabela hash com encadeamento separado (separate chaining).
 *
 * - Colisões: cada bucket é uma lista simplesmente encadeada de nós.
 * - Fator de carga: size() / bucketCount(), mantido <= maxLoadFactor().
 * - Redimensionamento: dobra o número de buckets ao exceder o fator de carga
 *   máximo e reduz pela metade quando a tabela fica muito esparsa
 *   (nunca abaixo da capacidade inicial).
 *
 * Os tipos Key e Value são aliases para facilitar a troca de tipos.
 */
class HashTable {
public:
    using Key = std::string;
    using Value = int;

    static constexpr std::size_t kDefaultInitialCapacity = 8;
    static constexpr double kDefaultMaxLoadFactor = 0.75;

    /// Lança std::invalid_argument se initialCapacity == 0.
    explicit HashTable(std::size_t initialCapacity = kDefaultInitialCapacity);
    ~HashTable();

    HashTable(const HashTable& other);
    HashTable& operator=(const HashTable& other);
    HashTable(HashTable&& other) noexcept;
    HashTable& operator=(HashTable&& other) noexcept;

    /// Insere o par; se a chave já existe, atualiza o valor.
    /// Retorna true se uma nova chave foi inserida, false se houve atualização.
    bool insert(const Key& key, const Value& value);

    /// Busca o valor associado à chave; std::nullopt se não existir.
    std::optional<Value> search(const Key& key) const;

    /// Indica se a chave existe na tabela.
    bool contains(const Key& key) const;

    /// Remove a chave. Retorna true se ela existia.
    bool remove(const Key& key);

    /// Remove todos os elementos, mantendo a capacidade atual.
    void clear();

    std::size_t size() const noexcept;
    bool empty() const noexcept;
    std::size_t bucketCount() const noexcept;

    /// Fator de carga atual (size / bucketCount).
    double loadFactor() const noexcept;
    double maxLoadFactor() const noexcept;

    /// Define o fator de carga máximo e redimensiona se necessário.
    /// Lança std::invalid_argument se maxLoad <= 0.
    void setMaxLoadFactor(double maxLoad);

    /// Redimensiona para pelo menos newBucketCount buckets, respeitando o
    /// fator de carga máximo, e redistribui todos os elementos.
    void rehash(std::size_t newBucketCount);

    void swap(HashTable& other) noexcept;

private:
    struct Node {
        Key key;
        Value value;
        Node* next;
    };

    static std::size_t hashKey(const Key& key) noexcept;

    std::size_t indexFor(const Key& key) const noexcept;
    Node* findNode(const Key& key) const noexcept;

    void ensureBuckets();
    void growIfNeeded(std::size_t prospectiveSize);
    void shrinkIfNeeded();
    void destroyAllNodes() noexcept;
    void copyNodesFrom(const HashTable& other);

    std::vector<Node*> buckets_;
    std::size_t size_;
    std::size_t initialCapacity_;
    double maxLoadFactor_;
};

#endif // HASH_TABLE
