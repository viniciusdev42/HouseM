#include "BTree.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Nó (detalhe de implementação, visível apenas neste arquivo)
// ---------------------------------------------------------------------------
struct BTree::Node {
    explicit Node(bool isLeaf) : leaf(isLeaf) {}

    bool leaf;
    std::vector<Entry> entries;
    std::vector<std::unique_ptr<Node>> children;
};

// ---------------------------------------------------------------------------
// Construção / destruição
// ---------------------------------------------------------------------------
BTree::BTree(std::size_t minimumDegree)
    : minimumDegree_(minimumDegree), size_(0), root_(nullptr) {
    if (minimumDegree_ < 2) {
        throw std::invalid_argument("BTree: o grau minimo deve ser >= 2");
    }
    root_ = std::make_unique<Node>(true);
}

BTree::~BTree() = default;

// ---------------------------------------------------------------------------
// Consultas públicas
// ---------------------------------------------------------------------------
std::optional<BTree::Value> BTree::search(Key key) const {
    const Entry* entry = findEntry(key);
    if (entry == nullptr) {
        return std::nullopt;
    }
    return entry->value;
}

bool BTree::update(Key key, Value value) {
    Entry* entry = findEntry(key);
    if (entry == nullptr) {
        return false;
    }
    entry->value = std::move(value);
    return true;
}

std::optional<BTree::Entry> BTree::findMin() const {
    if (empty()) {
        return std::nullopt;
    }
    return leftmostEntry(root_.get());
}

std::optional<BTree::Entry> BTree::findMax() const {
    if (empty()) {
        return std::nullopt;
    }
    return rightmostEntry(root_.get());
}

void BTree::traverse(
    const std::function<void(const Key&, const Value&)>& visit) const {
    traverseNode(root_.get(), visit);
}

std::vector<BTree::Entry> BTree::rangeSearch(Key lower, Key upper) const {
    if (lower > upper) {
        throw std::invalid_argument("BTree::rangeSearch: lower > upper");
    }
    std::vector<Entry> result;
    collectRange(root_.get(), lower, upper, result);
    return result;
}

std::size_t BTree::size() const noexcept { return size_; }

bool BTree::empty() const noexcept { return size_ == 0; }

// ---------------------------------------------------------------------------
// Inserção
// ---------------------------------------------------------------------------
bool BTree::insert(Key key, Value value) {
    if (findEntry(key) != nullptr) {
        return false;
    }

    // Raiz cheia: cria nova raiz e divide a antiga (única forma de a árvore crescer).
    if (root_->entries.size() == maxKeys()) {
        auto newRoot = std::make_unique<Node>(false);
        newRoot->children.push_back(std::move(root_));
        root_ = std::move(newRoot);
        splitChild(root_.get(), 0);
    }

    insertNonFull(root_.get(), Entry{key, std::move(value)});
    ++size_;
    return true;
}

void BTree::insertNonFull(Node* node, Entry entry) {
    std::size_t index = lowerBound(node, entry.key);

    if (node->leaf) {
        node->entries.insert(node->entries.begin() + static_cast<std::ptrdiff_t>(index),
                             std::move(entry));
        return;
    }

    if (node->children[index]->entries.size() == maxKeys()) {
        splitChild(node, index);
        if (entry.key > node->entries[index].key) {
            ++index;
        }
    }
    insertNonFull(node->children[index].get(), std::move(entry));
}

void BTree::splitChild(Node* parent, std::size_t childIndex) {
    const std::size_t t = minimumDegree_;
    Node* full = parent->children[childIndex].get();
    auto sibling = std::make_unique<Node>(full->leaf);

    // Chaves [t, 2t-2] vão para o novo irmão; a mediana (t-1) sobe.
    sibling->entries.assign(std::make_move_iterator(full->entries.begin() + static_cast<std::ptrdiff_t>(t)),
                            std::make_move_iterator(full->entries.end()));
    Entry median = std::move(full->entries[t - 1]);
    full->entries.resize(t - 1, Entry{});

    if (!full->leaf) {
        sibling->children.assign(
            std::make_move_iterator(full->children.begin() + static_cast<std::ptrdiff_t>(t)),
            std::make_move_iterator(full->children.end()));
        full->children.resize(t);
    }

    parent->entries.insert(parent->entries.begin() + static_cast<std::ptrdiff_t>(childIndex),
                           std::move(median));
    parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(childIndex) + 1,
                            std::move(sibling));
}

// ---------------------------------------------------------------------------
// Remoção
// ---------------------------------------------------------------------------
bool BTree::remove(Key key) {
    const bool removed = removeFromNode(root_.get(), key);
    collapseRoot();
    if (removed) {
        --size_;
    }
    return removed;
}

bool BTree::removeFromNode(Node* node, Key key) {
    std::size_t index = lowerBound(node, key);
    const bool found = index < node->entries.size() && node->entries[index].key == key;

    if (found) {
        if (node->leaf) {
            node->entries.erase(node->entries.begin() + static_cast<std::ptrdiff_t>(index));
            return true;
        }
        return removeFromInternal(node, index);
    }

    if (node->leaf) {
        return false;
    }

    // Descida preventiva: o filho precisa ter ao menos t chaves antes de entrar nele.
    if (node->children[index]->entries.size() < minimumDegree_) {
        index = rebalance(node, index);
    }
    return removeFromNode(node->children[index].get(), key);
}

bool BTree::removeFromInternal(Node* node, std::size_t keyIndex) {
    if (promoteKey(node, keyIndex)) {
        return true;
    }

    // Ambos os filhos adjacentes têm t-1 chaves: funde e remove da subárvore resultante.
    const Key key = node->entries[keyIndex].key;
    mergeChildren(node, keyIndex);
    return removeFromNode(node->children[keyIndex].get(), key);
}

bool BTree::promoteKey(Node* node, std::size_t keyIndex) {
    Node* left = node->children[keyIndex].get();
    Node* right = node->children[keyIndex + 1].get();

    if (left->entries.size() >= minimumDegree_) {
        Entry predecessor = rightmostEntry(left);
        node->entries[keyIndex] = predecessor;
        removeFromNode(left, predecessor.key);
        return true;
    }

    if (right->entries.size() >= minimumDegree_) {
        Entry successor = leftmostEntry(right);
        node->entries[keyIndex] = successor;
        removeFromNode(right, successor.key);
        return true;
    }

    return false;
}

void BTree::mergeChildren(Node* parent, std::size_t leftIndex) {
    Node* left = parent->children[leftIndex].get();
    std::unique_ptr<Node> right = std::move(parent->children[leftIndex + 1]);

    left->entries.push_back(std::move(parent->entries[leftIndex]));
    left->entries.insert(left->entries.end(),
                         std::make_move_iterator(right->entries.begin()),
                         std::make_move_iterator(right->entries.end()));

    if (!left->leaf) {
        left->children.insert(left->children.end(),
                              std::make_move_iterator(right->children.begin()),
                              std::make_move_iterator(right->children.end()));
    }

    parent->entries.erase(parent->entries.begin() + static_cast<std::ptrdiff_t>(leftIndex));
    parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(leftIndex) + 1);
}

void BTree::borrowFromLeft(Node* parent, std::size_t childIndex) {
    Node* child = parent->children[childIndex].get();
    Node* left = parent->children[childIndex - 1].get();

    child->entries.insert(child->entries.begin(), std::move(parent->entries[childIndex - 1]));
    parent->entries[childIndex - 1] = std::move(left->entries.back());
    left->entries.pop_back();

    if (!child->leaf) {
        child->children.insert(child->children.begin(), std::move(left->children.back()));
        left->children.pop_back();
    }
}

void BTree::borrowFromRight(Node* parent, std::size_t childIndex) {
    Node* child = parent->children[childIndex].get();
    Node* right = parent->children[childIndex + 1].get();

    child->entries.push_back(std::move(parent->entries[childIndex]));
    parent->entries[childIndex] = std::move(right->entries.front());
    right->entries.erase(right->entries.begin());

    if (!child->leaf) {
        child->children.push_back(std::move(right->children.front()));
        right->children.erase(right->children.begin());
    }
}

std::size_t BTree::rebalance(Node* parent, std::size_t childIndex) {
    const bool hasLeft = childIndex > 0;
    const bool hasRight = childIndex + 1 < parent->children.size();

    if (hasLeft && parent->children[childIndex - 1]->entries.size() >= minimumDegree_) {
        borrowFromLeft(parent, childIndex);
        return childIndex;
    }

    if (hasRight && parent->children[childIndex + 1]->entries.size() >= minimumDegree_) {
        borrowFromRight(parent, childIndex);
        return childIndex;
    }

    if (hasRight) {
        mergeChildren(parent, childIndex);
        return childIndex;
    }

    mergeChildren(parent, childIndex - 1);
    return childIndex - 1;
}

void BTree::collapseRoot() {
    if (root_->entries.empty() && !root_->leaf) {
        std::unique_ptr<Node> newRoot = std::move(root_->children.front());
        root_ = std::move(newRoot);
    }
}

// ---------------------------------------------------------------------------
// Auxiliares
// ---------------------------------------------------------------------------
BTree::Entry* BTree::findEntry(Key key) const {
    Node* node = root_.get();
    while (node != nullptr) {
        const std::size_t index = lowerBound(node, key);
        if (index < node->entries.size() && node->entries[index].key == key) {
            return &node->entries[index];
        }
        if (node->leaf) {
            return nullptr;
        }
        node = node->children[index].get();
    }
    return nullptr;
}

std::size_t BTree::lowerBound(const Node* node, Key key) {
    const auto it = std::lower_bound(
        node->entries.begin(), node->entries.end(), key,
        [](const Entry& entry, Key k) { return entry.key < k; });
    return static_cast<std::size_t>(std::distance(node->entries.begin(), it));
}

const BTree::Entry& BTree::leftmostEntry(const Node* node) {
    while (!node->leaf) {
        node = node->children.front().get();
    }
    return node->entries.front();
}

const BTree::Entry& BTree::rightmostEntry(const Node* node) {
    while (!node->leaf) {
        node = node->children.back().get();
    }
    return node->entries.back();
}

void BTree::traverseNode(
    const Node* node,
    const std::function<void(const Key&, const Value&)>& visit) const {
    for (std::size_t i = 0; i < node->entries.size(); ++i) {
        if (!node->leaf) {
            traverseNode(node->children[i].get(), visit);
        }
        visit(node->entries[i].key, node->entries[i].value);
    }
    if (!node->leaf) {
        traverseNode(node->children.back().get(), visit);
    }
}

// Retorna false quando uma chave > upper foi encontrada (encerra a busca cedo).
bool BTree::collectRange(const Node* node, Key lower, Key upper,
                         std::vector<Entry>& out) const {
    std::size_t i = lowerBound(node, lower);

    for (; i < node->entries.size(); ++i) {
        if (!node->leaf && !collectRange(node->children[i].get(), lower, upper, out)) {
            return false;
        }
        if (node->entries[i].key > upper) {
            return false;
        }
        out.push_back(node->entries[i]);
    }

    if (!node->leaf) {
        return collectRange(node->children[i].get(), lower, upper, out);
    }
    return true;
}
