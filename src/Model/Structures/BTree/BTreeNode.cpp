#include "BTreeNode.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

// ---------------------------------------------------------------------------
// Construção
// ---------------------------------------------------------------------------

template <typename Key, typename Compare>
BTreeNode<Key, Compare>::BTreeNode(size_type minDegree, bool leaf, Compare comp)
    : minDegree_(minDegree), leaf_(leaf), comp_(std::move(comp)) {
    if (minDegree_ < 2) {
        throw std::invalid_argument("BTreeNode: o grau minimo deve ser >= 2");
    }
    keys_.reserve(maxKeys());
    if (!leaf_) {
        children_.reserve(maxChildren());
    }
}

// ---------------------------------------------------------------------------
// Consultas de estado
// ---------------------------------------------------------------------------

template <typename Key, typename Compare>
bool BTreeNode<Key, Compare>::isLeaf() const noexcept {
    return leaf_;
}

template <typename Key, typename Compare>
bool BTreeNode<Key, Compare>::isFull() const noexcept {
    return keys_.size() >= maxKeys();
}

template <typename Key, typename Compare>
bool BTreeNode<Key, Compare>::isEmpty() const noexcept {
    return keys_.empty();
}

template <typename Key, typename Compare>
typename BTreeNode<Key, Compare>::size_type
BTreeNode<Key, Compare>::keyCount() const noexcept {
    return keys_.size();
}

template <typename Key, typename Compare>
typename BTreeNode<Key, Compare>::size_type
BTreeNode<Key, Compare>::childCount() const noexcept {
    return children_.size();
}

// ---------------------------------------------------------------------------
// Chaves
// ---------------------------------------------------------------------------

template <typename Key, typename Compare>
typename BTreeNode<Key, Compare>::size_type
BTreeNode<Key, Compare>::findKey(const Key& key, bool* found) const {
    const auto it = std::lower_bound(keys_.begin(), keys_.end(), key, comp_);
    const size_type index = static_cast<size_type>(it - keys_.begin());
    if (found != nullptr) {
        *found = (it != keys_.end()) && !comp_(key, *it);
    }
    return index;
}

template <typename Key, typename Compare>
typename BTreeNode<Key, Compare>::size_type
BTreeNode<Key, Compare>::insertKey(Key key) {
    if (isFull()) {
        throw std::length_error("BTreeNode::insertKey: no cheio");
    }
    bool found = false;
    const size_type index = findKey(key, &found);
    if (found) {
        throw std::invalid_argument("BTreeNode::insertKey: chave duplicada");
    }
    keys_.insert(keys_.begin() + static_cast<std::ptrdiff_t>(index),
                 std::move(key));
    return index;
}

template <typename Key, typename Compare>
Key BTreeNode<Key, Compare>::removeKey(size_type index) {
    if (index >= keys_.size()) {
        throw std::out_of_range("BTreeNode::removeKey: indice fora do intervalo");
    }
    Key removed = std::move(keys_[index]);
    keys_.erase(keys_.begin() + static_cast<std::ptrdiff_t>(index));
    return removed;
}

template <typename Key, typename Compare>
const Key& BTreeNode<Key, Compare>::getKey(size_type index) const {
    if (index >= keys_.size()) {
        throw std::out_of_range("BTreeNode::getKey: indice fora do intervalo");
    }
    return keys_[index];
}

// ---------------------------------------------------------------------------
// Filhos
// ---------------------------------------------------------------------------

template <typename Key, typename Compare>
void BTreeNode<Key, Compare>::insertChild(size_type index, NodePtr child) {
    if (leaf_) {
        throw std::logic_error("BTreeNode::insertChild: folhas nao possuem filhos");
    }
    if (!child) {
        throw std::invalid_argument("BTreeNode::insertChild: filho nulo");
    }
    if (child->minDegree_ != minDegree_) {
        throw std::invalid_argument(
            "BTreeNode::insertChild: grau minimo do filho difere do pai");
    }
    if (children_.size() >= maxChildren()) {
        throw std::length_error("BTreeNode::insertChild: numero maximo de filhos atingido");
    }
    if (index > children_.size()) {
        throw std::out_of_range("BTreeNode::insertChild: indice fora do intervalo");
    }
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index),
                     std::move(child));
}

template <typename Key, typename Compare>
typename BTreeNode<Key, Compare>::NodePtr
BTreeNode<Key, Compare>::removeChild(size_type index) {
    if (index >= children_.size()) {
        throw std::out_of_range("BTreeNode::removeChild: indice fora do intervalo");
    }
    NodePtr removed = std::move(children_[index]);
    children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(index));
    return removed;
}

template <typename Key, typename Compare>
BTreeNode<Key, Compare>* BTreeNode<Key, Compare>::getChild(size_type index) {
    if (index >= children_.size()) {
        throw std::out_of_range("BTreeNode::getChild: indice fora do intervalo");
    }
    return children_[index].get();
}

template <typename Key, typename Compare>
const BTreeNode<Key, Compare>*
BTreeNode<Key, Compare>::getChild(size_type index) const {
    if (index >= children_.size()) {
        throw std::out_of_range("BTreeNode::getChild: indice fora do intervalo");
    }
    return children_[index].get();
}

// ---------------------------------------------------------------------------
// Manutenção
// ---------------------------------------------------------------------------

template <typename Key, typename Compare>
void BTreeNode<Key, Compare>::clear() noexcept {
    keys_.clear();
    children_.clear();  // unique_ptr destrói recursivamente as subárvores
}

template <typename Key, typename Compare>
bool BTreeNode<Key, Compare>::validate(std::string* error) const {
    long leafDepth = -1;  // profundidade da primeira folha encontrada
    return validateSubtree(nullptr, nullptr, 0, leafDepth, error);
}

template <typename Key, typename Compare>
bool BTreeNode<Key, Compare>::validateSubtree(const Key* lowerBound,
                                              const Key* upperBound,
                                              size_type depth, long& leafDepth,
                                              std::string* error) const {
    const auto fail = [error](const char* message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (keys_.size() > maxKeys()) {
        return fail("no excede o numero maximo de chaves (2t - 1)");
    }

    for (size_type i = 0; i < keys_.size(); ++i) {
        if (i > 0 && !comp_(keys_[i - 1], keys_[i])) {
            return fail("chaves fora de ordem ou duplicadas");
        }
        if (lowerBound != nullptr && !comp_(*lowerBound, keys_[i])) {
            return fail("chave menor ou igual ao separador inferior do pai");
        }
        if (upperBound != nullptr && !comp_(keys_[i], *upperBound)) {
            return fail("chave maior ou igual ao separador superior do pai");
        }
    }

    if (leaf_) {
        if (!children_.empty()) {
            return fail("no folha possui filhos");
        }
        if (leafDepth < 0) {
            leafDepth = static_cast<long>(depth);
        } else if (leafDepth != static_cast<long>(depth)) {
            return fail("folhas em profundidades diferentes");
        }
        return true;
    }

    if (keys_.empty() && children_.empty()) {
        return true;  // no interno recem-criado, ainda sem conteudo
    }
    if (children_.size() != keys_.size() + 1) {
        return fail("no interno deve ter (numero de chaves + 1) filhos");
    }

    for (size_type i = 0; i < children_.size(); ++i) {
        const BTreeNode* child = children_[i].get();
        if (child == nullptr) {
            return fail("filho nulo encontrado");
        }
        if (child->minDegree_ != minDegree_) {
            return fail("filho com grau minimo diferente do pai");
        }
        const Key* lo = (i == 0) ? lowerBound : &keys_[i - 1];
        const Key* hi = (i == keys_.size()) ? upperBound : &keys_[i];
        if (!child->validateSubtree(lo, hi, depth + 1, leafDepth, error)) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Auxiliares privados
// ---------------------------------------------------------------------------

template <typename Key, typename Compare>
typename BTreeNode<Key, Compare>::size_type
BTreeNode<Key, Compare>::maxKeys() const noexcept {
    return 2 * minDegree_ - 1;
}

template <typename Key, typename Compare>
typename BTreeNode<Key, Compare>::size_type
BTreeNode<Key, Compare>::maxChildren() const noexcept {
    return 2 * minDegree_;
}

