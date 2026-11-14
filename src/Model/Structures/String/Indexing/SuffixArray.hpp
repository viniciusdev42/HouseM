/**
 * Suffix Array construído com o algoritmo Prefix Doubling (Manber & Myers).
 *
 * Ideia: ordenamos os sufixos pelos primeiros k caracteres e, a cada rodada,
 * dobramos k (1, 2, 4, ...). O ranking dos 2k primeiros caracteres de um
 * sufixo i é o par (rank[i], rank[i + k]), que já é conhecido da rodada anterior.
 * Cada rodada usa Counting Sort (radix de 2 passes), logo:
 *
 *   Tempo:   O(n log n)
 *   Memória: O(n + sigma), com sigma = 256
 *
 * Além do vetor de sufixos, mantém o vetor de ranks (inverso do SA) e o
 * vetor LCP calculado pelo algoritmo de Kasai em O(n).
 */

#ifndef SUFFIX_ARRAY
#define SUFIX_ARRAY

#include <algorithm>
#include <cstddef>
#include <vector>

#include "src/Model/Structures/String/Core/StringView/StringView.hpp"

using SuffixView = StringView::StringView;

class SuffixArray {
public:
    using size_type = std::size_t;

    explicit SuffixArray(SuffixView text)
        : text_(text) {
        buildSuffixArray();
        buildLcp();
    }
    
    size_type size() const noexcept;
    
    int operator[](size_type i) const;

    const std::vector<int>& suffixArray() const noexcept;
    
    const std::vector<int>& rank() const noexcept;

    const std::vector<int>& lcp() const noexcept;

private:
    static constexpr int kAlphabetSize = 256;
    
    SuffixView text_;
    std::vector<int> sa_;
    std::vector<int> rank_;
    std::vector<int> lcp_;

    void buildSuffixArray();
    
    int sortByFirstCharacter(std::vector<int>& counter);
  
    void orderBySecondKey(int k, std::vector<int>& out);
    
    void countingSortByFirstKey(const std::vector<int>& bySecondKey,int classes,std::vector<int>& counter);
        
    int recomputeRanks(int k, std::vector<int>& newRank)const;
    
    int secondKey(int i, int k) const;
    
    int charAt(int i) const;
    
    void buildLcp();
    
};    
#endif // SUFFIX_ARRAY
