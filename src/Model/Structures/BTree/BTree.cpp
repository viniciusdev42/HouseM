#include "BTree.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

// ===========================================================================
// Ciclo de vida
// ===========================================================================

BTree::BTree(std::size_t minDegree) : minDegree_(minDegree) {
    if (minDegree < 2) {
        throw std::invalid_argument("BTree: o grau minimo deve ser >= 2");
    }
}

BTree::~BTree() { clear(); }

BTree::BTree(BTree&& other) noexcept
    : minDegree_(other.minDegree_),
      root_(std::exchange(other.root_, nullptr)),
      size_(std::exchange(other.size_, 0)) {}

BTree& BTree::operator=(BTree&& other) noexcept {
    if (this != &other) {
        clear();
        minDegree_ = other.minDegree_;
        root_ = std::exchange(other.root_, nullptr);
        size_ = std::exchange(other.size_, 0);
    }
    return *this;
}

void BTree::destroySubtree(BTreeNode* node) noexcept {
    if (node == nullptr) {
        return;
    }
    while (node->childCount() > 0) {
        destroySubtree(node->removeChild(node->childCount() - 1));
    }
    delete node;
}

void BTree::clear() noexcept {
    destroySubtree(root_);
    root_ = nullptr;
    size_ = 0;
}

// ===========================================================================
// Estado
// ===========================================================================

bool BTree::empty() const noexcept { return size_ == 0; }
std::size_t BTree::size() const noexcept { return size_; }
std::size_t BTree::minDegree() const noexcept { return minDegree_; }

std::size_t BTree::height() const noexcept {
    std::size_t levels = 0;
    for (const BTreeNode* node = root_; node != nullptr;
         node = node->isLeaf() ? nullptr : node->getChild(0)) {
        ++levels;
    }
    return levels;
}

// ===========================================================================
// Busca
// ===========================================================================

bool BTree::contains(const Key& key) const {
    const BTreeNode* node = root_;
    while (node != nullptr) {
        const std::size_t index = node->lowerBound(key);
        if (index < node->keyCount() && !(key < node->getKey(index))) {
            return true;
        }
        node = node->isLeaf() ? nullptr : node->getChild(index);
    }
    return false;
}

std::optional<BTree::Key> BTree::findMin() const {
    if (root_ == nullptr) {
        return std::nullopt;
    }
    const BTreeNode* node = root_;
    while (!node->isLeaf()) {
        node = node->getChild(0);
    }
    return node->getKey(0);
}

std::optional<BTree::Key> BTree::findMax() const {
    if (root_ == nullptr) {
        return std::nullopt;
    }
    const BTreeNode* node = root_;
    while (!node->isLeaf()) {
        node = node->getChild(node->childCount() - 1);
    }
    return node->getKey(node->keyCount() - 1);
}

std::optional<BTree::Key> BTree::lowerBound(const Key& key) const {
    std::optional<Key> candidate;
    const BTreeNode* node = root_;
    while (node != nullptr) {
        const std::size_t index = node->lowerBound(key);
        if (index < node->keyCount()) {
            candidate = node->getKey(index);
            if (!(key < *candidate)) {
                return candidate;  // chave igual encontrada
            }
        }
        node = node->isLeaf() ? nullptr : node->getChild(index);
    }
    return candidate;
}

std::optional<BTree::Key> BTree::upperBound(const Key& key) const {
    std::optional<Key> candidate;
    const BTreeNode* node = root_;
    while (node != nullptr) {
        std::size_t index = node->lowerBound(key);
        // Se a chave existe neste nó, a primeira estritamente maior está
        // à direita dela.
        if (index < node->keyCount() && !(key < node->getKey(index))) {
            ++index;
        }
        if (index < node->keyCount()) {
            candidate = node->getKey(index);
        }
        node = node->isLeaf() ? nullptr : node->getChild(index);
    }
    return candidate;
}

std::vector<BTree::Key> BTree::rangeSearch(const Key& low,
                                           const Key& high) const {
    std::vector<Key> result;
    if (root_ != nullptr && !(high < low)) {
        rangeSearchNode(root_, low, high, result);
    }
    return result;
}

void BTree::rangeSearchNode(const BTreeNode* node, const Key& low,
                            const Key& high, std::vector<Key>& out) {
    const std::size_t count = node->keyCount();
    for (std::size_t i = node->lowerBound(low); i < count; ++i) {
        if (!node->isLeaf()) {
            rangeSearchNode(node->getChild(i), low, high, out);
        }
        if (high < node->getKey(i)) {
            return;  // todo o resto é maior que `high`
        }
        out.push_back(node->getKey(i));
    }
    if (!node->isLeaf()) {
        rangeSearchNode(node->getChild(count), low, high, out);
    }
}

void BTree::traverseInOrder(const std::function<void(const Key&)>& visit) const {
    if (root_ != nullptr && visit) {
        traverseNode(root_, visit);
    }
}

void BTree::traverseNode(const BTreeNode* node,
                         const std::function<void(const Key&)>& visit) {
    const std::size_t count = node->keyCount();
    for (std::size_t i = 0; i < count; ++i) {
        if (!node->isLeaf()) {
            traverseNode(node->getChild(i), visit);
        }
        visit(node->getKey(i));
    }
    if (!node->isLeaf()) {
        traverseNode(node->getChild(count), visit);
    }
}

// ===========================================================================
// Inserção
// ===========================================================================

bool BTree::insert(const Key& key) {
    if (contains(key)) {
        return false;
    }

    if (root_ == nullptr) {
        auto leaf = std::make_unique<BTreeNode>(minDegree_, true);
        leaf->insertKey(key);
        root_ = leaf.release();
        size_ = 1;
        return true;
    }

    if (root_->isFull()) {
        // A árvore cresce em altura apenas aqui, pela raiz.
        auto newRoot = std::make_unique<BTreeNode>(minDegree_, false);
        newRoot->insertChild(0, root_);
        splitChild(newRoot.get(), 0);
        root_ = newRoot.release();
    }

    insertNonFull(root_, key);
    ++size_;
    return true;
}

void BTree::splitChild(BTreeNode* parent, std::size_t index) {
    const std::size_t t = minDegree_;
    BTreeNode* child = parent->getChild(index);
    auto sibling = std::make_unique<BTreeNode>(t, child->isLeaf());

    // Chaves [t, 2t-2] vão para o irmão; a chave t-1 (mediana) sobe.
    for (std::size_t j = 0; j < t - 1; ++j) {
        sibling->insertKey(child->getKey(t + j));
    }
    if (!child->isLeaf()) {
        for (std::size_t j = 0; j < t; ++j) {
            sibling->insertChild(j, child->getChild(t + j));
        }
    }

    for (std::size_t j = 0; j < t - 1; ++j) {
        child->removeKey(child->keyCount() - 1);
    }
    if (!child->isLeaf()) {
        for (std::size_t j = 0; j < t; ++j) {
            child->removeChild(child->childCount() - 1);
        }
    }
    const Key median = child->removeKey(t - 1);

    parent->insertChild(index + 1, sibling.get());
    sibling.release();
    parent->insertKey(median);
}

void BTree::insertNonFull(BTreeNode* node, const Key& key) {
    while (!node->isLeaf()) {
        std::size_t index = node->lowerBound(key);
        if (node->getChild(index)->isFull()) {
            splitChild(node, index);
            if (node->getKey(index) < key) {
                ++index;
            }
        }
        node = node->getChild(index);
    }
    node->insertKey(key);
}

// ===========================================================================
// Remoção
// ===========================================================================

bool BTree::remove(const Key& key) {
    // Verificar antes evita reestruturar a árvore numa remoção que falharia.
    if (!contains(key)) {
        return false;
    }
    removeFromNode(root_, key);
    --size_;
    collapseRoot();
    return true;
}

void BTree::removeFromNode(BTreeNode* node, const Key& key) {
    const std::size_t index = node->lowerBound(key);
    const bool found =
        index < node->keyCount() && !(key < node->getKey(index));

    if (found) {
        if (node->isLeaf()) {
            node->removeKey(index);
        } else {
            removeFromInternalNode(node, index);
        }
        return;
    }

    // A chave existe (pré-condição), logo não pode estar ausente numa folha.
    std::size_t childIndex = index;
    if (node->getChild(childIndex)->keyCount() < minDegree_) {
        childIndex = fillChild(node, childIndex);
    }
    removeFromNode(node->getChild(childIndex), key);
}

void BTree::removeFromInternalNode(BTreeNode* node, std::size_t index) {
    const Key key = node->getKey(index);
    BTreeNode* left = node->getChild(index);
    BTreeNode* right = node->getChild(index + 1);

    if (left->keyCount() >= minDegree_) {
        const Key predecessor = getPredecessor(node, index);
        node->removeKey(index);
        node->insertKey(predecessor);  // mantém a posição: está entre vizinhas
        removeFromNode(left, predecessor);
    } else if (right->keyCount() >= minDegree_) {
        const Key successor = getSuccessor(node, index);
        node->removeKey(index);
        node->insertKey(successor);
        removeFromNode(right, successor);
    } else {
        mergeChildren(node, index);
        removeFromNode(node->getChild(index), key);
    }
}

BTree::Key BTree::getPredecessor(const BTreeNode* node,
                                 std::size_t index) const {
    const BTreeNode* current = node->getChild(index);
    while (!current->isLeaf()) {
        current = current->getChild(current->childCount() - 1);
    }
    return current->getKey(current->keyCount() - 1);
}

BTree::Key BTree::getSuccessor(const BTreeNode* node,
                               std::size_t index) const {
    const BTreeNode* current = node->getChild(index + 1);
    while (!current->isLeaf()) {
        current = current->getChild(0);
    }
    return current->getKey(0);
}

std::size_t BTree::fillChild(BTreeNode* parent, std::size_t index) {
    const bool hasPrevious = index > 0;
    const bool hasNext = index < parent->keyCount();  // filho index+1 existe

    if (hasPrevious && parent->getChild(index - 1)->keyCount() >= minDegree_) {
        borrowFromPrevious(parent, index);
        return index;
    }
    if (hasNext && parent->getChild(index + 1)->keyCount() >= minDegree_) {
        borrowFromNext(parent, index);
        return index;
    }
    if (hasNext) {
        mergeChildren(parent, index);
        return index;
    }
    mergeChildren(parent, index - 1);
    return index - 1;
}

void BTree::borrowFromPrevious(BTreeNode* parent, std::size_t index) {
    BTreeNode* child = parent->getChild(index);
    BTreeNode* sibling = parent->getChild(index - 1);

    // Separador desce para o filho; a maior chave do irmão sobe.
    const Key separator = parent->removeKey(index - 1);
    child->insertKey(separator);
    const Key siblingKey = sibling->removeKey(sibling->keyCount() - 1);
    parent->insertKey(siblingKey);

    if (!child->isLeaf()) {
        BTreeNode* moved = sibling->removeChild(sibling->childCount() - 1);
        child->insertChild(0, moved);
    }
}

void BTree::borrowFromNext(BTreeNode* parent, std::size_t index) {
    BTreeNode* child = parent->getChild(index);
    BTreeNode* sibling = parent->getChild(index + 1);

    // Separador desce para o filho; a menor chave do irmão sobe.
    const Key separator = parent->removeKey(index);
    child->insertKey(separator);
    const Key siblingKey = sibling->removeKey(0);
    parent->insertKey(siblingKey);

    if (!child->isLeaf()) {
        BTreeNode* moved = sibling->removeChild(0);
        child->insertChild(child->childCount(), moved);
    }
}

void BTree::mergeChildren(BTreeNode* parent, std::size_t index) {
    BTreeNode* left = parent->getChild(index);
    std::unique_ptr<BTreeNode> right(parent->removeChild(index + 1));

    const Key separator = parent->removeKey(index);
    left->insertKey(separator);
    for (std::size_t j = 0; j < right->keyCount(); ++j) {
        left->insertKey(right->getKey(j));
    }
    while (right->childCount() > 0) {
        left->insertChild(left->childCount(), right->removeChild(0));
    }
    // `right` é destruído aqui; seus filhos já pertencem a `left`.
}

void BTree::collapseRoot() {
    if (root_ == nullptr || !root_->isEmpty()) {
        return;
    }
    BTreeNode* oldRoot = root_;
    root_ = oldRoot->isLeaf() ? nullptr : oldRoot->removeChild(0);
    delete oldRoot;
}
