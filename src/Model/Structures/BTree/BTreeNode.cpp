#include "BTreeNode.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

void checkIndex(std::size_t index, std::size_t limit, const char* what) {
    if (index >= limit) {
        throw std::out_of_range(std::string("BTreeNode: indice de ") + what +
                                " fora do intervalo (" + std::to_string(index) +
                                " >= " + std::to_string(limit) + ")");
    }
}

bool fail(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

} // namespace

BTreeNode::BTreeNode(std::size_t minDegree, bool leaf)
    : minDegree_(minDegree), leaf_(leaf) {
    if (minDegree < 2) {
        throw std::invalid_argument("BTreeNode: o grau minimo deve ser >= 2");
    }
    // Evita realocações: o nó nunca excede essas capacidades.
    keys_.reserve(maxKeys());
    if (!leaf_) {
        children_.reserve(maxChildren());
    }
}

BTreeNode::BTreeNode(std::size_t minDegree, bool leaf)
    : minDegree_(minDegree), leaf_(leaf) {
    if (minDegree < 2) {
        throw std::invalid_argument("BTreeNode: o grau minimo deve ser >= 2");
    }
    keys_.reserve(maxKeys());
    if (!leaf_) {
        children_.reserve(maxChildren());
    }
}

// ---- Consultas de estado ----------------------------------------------

bool BTreeNode::isLeaf() const noexcept { return leaf_; }
bool BTreeNode::isFull() const noexcept { return keys_.size() == maxKeys(); }
bool BTreeNode::isEmpty() const noexcept { return keys_.empty(); }
std::size_t BTreeNode::keyCount() const noexcept { return keys_.size(); }
std::size_t BTreeNode::childCount() const noexcept { return children_.size(); }

std::size_t BTreeNode::minDegree() const noexcept { return minDegree_; }
std::size_t BTreeNode::maxKeys() const noexcept { return 2 * minDegree_ - 1; }
std::size_t BTreeNode::maxChildren() const noexcept { return 2 * minDegree_; }

// ---- Busca --------------------------------------------------------------

std::optional<std::size_t> BTreeNode::findKey(const Key& key) const {
    const std::size_t index = lowerBound(key);
    if (index < keys_.size() && !(key < keys_[index])) {
        return index;
    }
    return std::nullopt;
}

std::size_t BTreeNode::lowerBound(const Key& key) const {
    return static_cast<std::size_t>(
        std::lower_bound(keys_.begin(), keys_.end(), key) - keys_.begin());
}

// ---- Modificação de chaves ----------------------------------------------

std::size_t BTreeNode::insertKey(const Key& key) {
    if (isFull()) {
        throw std::length_error("BTreeNode::insertKey: no cheio");
    }
    const std::size_t index = lowerBound(key);
    if (index < keys_.size() && !(key < keys_[index])) {
        throw std::invalid_argument("BTreeNode::insertKey: chave duplicada");
    }
    keys_.insert(keys_.begin() + static_cast<std::ptrdiff_t>(index), key);
    return index;
}

BTreeNode::Key BTreeNode::removeKey(std::size_t index) {
    if (index >= keys_.size()) {
        throw std::out_of_range("BTreeNode::removeKey: indice invalido");
    }
    const Key removed = keys_[index];
    keys_.erase(keys_.begin() + static_cast<std::ptrdiff_t>(index));
    return removed;
}

// ---- Modificação de filhos ----------------------------------------------

void BTreeNode::insertChild(std::size_t index, BTreeNode* child) {
    if (leaf_) {
        throw std::logic_error("BTreeNode::insertChild: folha nao possui filhos");
    }
    if (child == nullptr) {
        throw std::invalid_argument("BTreeNode::insertChild: filho nulo");
    }
    if (index > children_.size()) {
        throw std::out_of_range("BTreeNode::insertChild: indice invalido");
    }
    if (children_.size() >= maxChildren()) {
        throw std::length_error("BTreeNode::insertChild: numero maximo de filhos");
    }
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), child);
}

BTreeNode* BTreeNode::removeChild(std::size_t index) {
    if (index >= children_.size()) {
        throw std::out_of_range("BTreeNode::removeChild: indice invalido");
    }
    BTreeNode* removed = children_[index];
    children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(index));
    return removed;
}

// ---- Acesso ---------------------------------------------------------------

const BTreeNode::Key& BTreeNode::getKey(std::size_t index) const {
    if (index >= keys_.size()) {
        throw std::out_of_range("BTreeNode::getKey: indice invalido");
    }
    return keys_[index];
}

BTreeNode* BTreeNode::getChild(std::size_t index) {
    if (index >= children_.size()) {
        throw std::out_of_range("BTreeNode::getChild: indice invalido");
    }
    return children_[index];
}

const BTreeNode* BTreeNode::getChild(std::size_t index) const {
    if (index >= children_.size()) {
        throw std::out_of_range("BTreeNode::getChild: indice invalido");
    }
    return children_[index];
}

void BTreeNode::clear() noexcept {
    keys_.clear();
    children_.clear();
}

// ---- Validação ------------------------------------------------------------

bool BTreeNode::validate(std::string* error) const {
    const auto fail = [error](const char* message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (keys_.size() > maxKeys()) {
        return fail("numero de chaves excede 2t - 1");
    }
    for (std::size_t i = 1; i < keys_.size(); ++i) {
        if (!(keys_[i - 1] < keys_[i])) {
            return fail("chaves nao estao estritamente crescentes");
        }
    }
    if (leaf_) {
        if (!children_.empty()) {
            return fail("no folha possui filhos");
        }
        return true;
    }
    if (children_.size() > maxChildren()) {
        return fail("numero de filhos excede 2t");
    }
    if (!(children_.empty() && keys_.empty()) &&
        children_.size() != keys_.size() + 1) {
        return fail("no interno deve ter keyCount() + 1 filhos");
    }
    for (const BTreeNode* child : children_) {
        if (child == nullptr) {
            return fail("no interno possui filho nulo");
        }
    }
    return true;
}
