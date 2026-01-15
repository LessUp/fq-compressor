/*
 * BooPHF library - Bloom filter-based minimal perfect hash function
 *
 * Intended to be a minimal perfect hash function with fast and low memory
 * construction, at the cost of (slightly) higher bits/elem than other state of
 * the art libraries once built.
 *
 * Should work with arbitrary large number of elements, based on a cascade of
 * "collision-free" bit arrays.
 *
 * Original source: https://github.com/rizkg/BBHash
 * Integrated into Spring for FASTQ compression
 *
 * Extracted for fq-compressor project - Non-commercial/Research use only
 */

#ifndef SPRING_CORE_BOOPHF_H_
#define SPRING_CORE_BOOPHF_H_

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace spring {
namespace boomphf {

// =============================================================================
// Utility Functions
// =============================================================================

/// Population count for 32-bit integer
inline unsigned int popcount_32(unsigned int x) {
    unsigned int m1 = 0x55555555;
    unsigned int m2 = 0x33333333;
    unsigned int m4 = 0x0f0f0f0f;
    unsigned int h01 = 0x01010101;
    x -= (x >> 1) & m1;
    x = (x & m2) + ((x >> 2) & m2);
    x = (x + (x >> 4)) & m4;
    return (x * h01) >> 24;
}

/// Population count for 64-bit integer
inline unsigned int popcount_64(uint64_t x) {
    unsigned int low = x & 0xffffffff;
    unsigned int high = static_cast<unsigned int>((x >> 32ULL) & 0xffffffff);
    return popcount_32(low) + popcount_32(high);
}

/// Fast range mapping: maps hash to [0, p) without modulo
inline uint64_t fastrange64(uint64_t word, uint64_t p) {
    return static_cast<uint64_t>(
        (static_cast<__uint128_t>(word) * static_cast<__uint128_t>(p)) >> 64);
}

// =============================================================================
// Hash Types
// =============================================================================

using hash_set_t = std::array<uint64_t, 10>;
using hash_pair_t = std::array<uint64_t, 2>;

// =============================================================================
// Hash Functors
// =============================================================================

/**
 * @brief Multiple hash function generator
 * @tparam Item Type of items to hash
 */
template <typename Item>
class HashFunctors {
public:
    HashFunctors() : nbFct_(7), userSeed_(0) {
        generateHashSeed();
    }

    /// Return one hash with specific seed index
    uint64_t operator()(const Item& key, size_t idx) const {
        return hash64(key, seedTab_[idx]);
    }

    /// Hash with custom seed
    uint64_t hashWithSeed(const Item& key, uint64_t seed) const {
        return hash64(key, seed);
    }

    /// Return all 10 hashes
    hash_set_t operator()(const Item& key) {
        hash_set_t hset;
        for (size_t ii = 0; ii < 10; ii++) {
            hset[ii] = hash64(key, seedTab_[ii]);
        }
        return hset;
    }

private:
    static uint64_t hash64(Item key, uint64_t seed) {
        uint64_t hash = seed;
        hash ^= (hash << 7) ^ key * (hash >> 3) ^
                (~((hash << 11) + (key ^ (hash >> 5))));
        hash = (~hash) + (hash << 21);
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8);
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4);
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
        return hash;
    }

    void generateHashSeed() {
        static const uint64_t rbase[kMaxNbFunc] = {
            0xAAAAAAAA55555555ULL, 0x33333333CCCCCCCCULL, 0x6666666699999999ULL,
            0xB5B5B5B54B4B4B4BULL, 0xAA55AA5555335533ULL, 0x33CC33CCCC66CC66ULL,
            0x6699669999B599B5ULL, 0xB54BB54B4BAA4BAAULL, 0xAA33AA3355CC55CCULL,
            0x33663366CC99CC99ULL};

        for (size_t i = 0; i < kMaxNbFunc; ++i) {
            seedTab_[i] = rbase[i];
        }
        for (size_t i = 0; i < kMaxNbFunc; ++i) {
            seedTab_[i] = seedTab_[i] * seedTab_[(i + 3) % kMaxNbFunc] + userSeed_;
        }
    }

    size_t nbFct_;
    static constexpr size_t kMaxNbFunc = 10;
    uint64_t seedTab_[kMaxNbFunc];
    uint64_t userSeed_;
};

/**
 * @brief Single hash functor wrapper
 * @tparam Item Type of items to hash
 */
template <typename Item>
class SingleHashFunctor {
public:
    uint64_t operator()(const Item& key,
                        uint64_t seed = 0xAAAAAAAA55555555ULL) const {
        return hashFunctors_.hashWithSeed(key, seed);
    }

private:
    HashFunctors<Item> hashFunctors_;
};

/**
 * @brief XorShift-based hash functor for generating multiple hashes efficiently
 * @tparam Item Type of items to hash
 * @tparam SingleHasher_t Single hash functor type
 */
template <typename Item, class SingleHasher_t>
class XorshiftHashFunctors {
public:
    uint64_t h0(hash_pair_t& s, const Item& key) {
        s[0] = singleHasher_(key, 0xAAAAAAAA55555555ULL);
        return s[0];
    }

    uint64_t h1(hash_pair_t& s, const Item& key) {
        s[1] = singleHasher_(key, 0x33333333CCCCCCCCULL);
        return s[1];
    }

    /// Return next hash and update state
    uint64_t next(hash_pair_t& s) {
        uint64_t s1 = s[0];
        const uint64_t s0 = s[1];
        s[0] = s0;
        s1 ^= s1 << 23;
        return (s[1] = (s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26))) + s0;
    }

    /// Return all hashes
    hash_set_t operator()(const Item& key) {
        uint64_t s[2];
        hash_set_t hset;

        hset[0] = singleHasher_(key, 0xAAAAAAAA55555555ULL);
        hset[1] = singleHasher_(key, 0x33333333CCCCCCCCULL);

        s[0] = hset[0];
        s[1] = hset[1];

        for (size_t ii = 2; ii < 10; ii++) {
            uint64_t s1 = s[0];
            const uint64_t s0 = s[1];
            s[0] = s0;
            s1 ^= s1 << 23;
            hset[ii] = (s[1] = (s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26))) + s0;
        }

        return hset;
    }

private:
    SingleHasher_t singleHasher_;
};

// =============================================================================
// Bit Vector
// =============================================================================

/**
 * @brief Compact bit vector with rank support
 */
class bitVector {
public:
    bitVector() : bitArray_(nullptr), size_(0), nchar_(0) {}

    explicit bitVector(uint64_t n) : size_(n) {
        nchar_ = 1ULL + n / 64ULL;
        bitArray_ = static_cast<uint64_t*>(calloc(nchar_, sizeof(uint64_t)));
    }

    ~bitVector() {
        if (bitArray_ != nullptr) {
            free(bitArray_);
        }
    }

    // Copy constructor
    bitVector(const bitVector& r) {
        size_ = r.size_;
        nchar_ = r.nchar_;
        ranks_ = r.ranks_;
        bitArray_ = static_cast<uint64_t*>(calloc(nchar_, sizeof(uint64_t)));
        memcpy(bitArray_, r.bitArray_, nchar_ * sizeof(uint64_t));
    }

    // Copy assignment
    bitVector& operator=(const bitVector& r) {
        if (&r != this) {
            size_ = r.size_;
            nchar_ = r.nchar_;
            ranks_ = r.ranks_;
            if (bitArray_ != nullptr) free(bitArray_);
            bitArray_ = static_cast<uint64_t*>(calloc(nchar_, sizeof(uint64_t)));
            memcpy(bitArray_, r.bitArray_, nchar_ * sizeof(uint64_t));
        }
        return *this;
    }

    // Move assignment
    bitVector& operator=(bitVector&& r) noexcept {
        if (&r != this) {
            if (bitArray_ != nullptr) free(bitArray_);
            size_ = std::move(r.size_);
            nchar_ = std::move(r.nchar_);
            ranks_ = std::move(r.ranks_);
            bitArray_ = r.bitArray_;
            r.bitArray_ = nullptr;
        }
        return *this;
    }

    // Move constructor
    bitVector(bitVector&& r) noexcept : bitArray_(nullptr), size_(0), nchar_(0) {
        *this = std::move(r);
    }

    void resize(uint64_t newsize) {
        nchar_ = 1ULL + newsize / 64ULL;
        bitArray_ = static_cast<uint64_t*>(
            realloc(bitArray_, nchar_ * sizeof(uint64_t)));
        size_ = newsize;
    }

    size_t size() const { return size_; }

    uint64_t bitSize() const {
        return nchar_ * 64ULL + ranks_.capacity() * 64ULL;
    }

    void clear() {
        memset(bitArray_, 0, nchar_ * sizeof(uint64_t));
    }

    void clearCollisions(uint64_t start, size_t size, bitVector* cc) {
        assert((start & 63) == 0);
        assert((size & 63) == 0);
        uint64_t ids = start / 64ULL;
        for (uint64_t ii = 0; ii < size / 64ULL; ii++) {
            bitArray_[ids + ii] = bitArray_[ids + ii] & (~(cc->get64(ii)));
        }
        cc->clear();
    }

    void clear(uint64_t start, size_t size) {
        assert((start & 63) == 0);
        assert((size & 63) == 0);
        memset(bitArray_ + (start / 64ULL), 0, (size / 64ULL) * sizeof(uint64_t));
    }

    uint64_t operator[](uint64_t pos) const {
        return (bitArray_[pos >> 6ULL] >> (pos & 63)) & 1;
    }

    uint64_t atomic_test_and_set(uint64_t pos) {
        uint64_t oldval = __sync_fetch_and_or(
            bitArray_ + (pos >> 6), 1ULL << (pos & 63));
        return (oldval >> (pos & 63)) & 1;
    }

    uint64_t get(uint64_t pos) const { return (*this)[pos]; }

    uint64_t get64(uint64_t cell64) const { return bitArray_[cell64]; }

    void set(uint64_t pos) {
        assert(pos < size_);
        __sync_fetch_and_or(bitArray_ + (pos >> 6ULL), 1ULL << (pos & 63));
    }

    void reset(uint64_t pos) {
        __sync_fetch_and_and(bitArray_ + (pos >> 6ULL), ~(1ULL << (pos & 63)));
    }

    uint64_t build_ranks(uint64_t offset = 0) {
        ranks_.reserve(2 + size_ / kNbBitsPerRankSample);

        uint64_t current_rank = offset;
        for (size_t ii = 0; ii < nchar_; ii++) {
            if (((ii * 64) % kNbBitsPerRankSample) == 0) {
                ranks_.push_back(current_rank);
            }
            current_rank += popcount_64(bitArray_[ii]);
        }

        return current_rank;
    }

    uint64_t rank(uint64_t pos) const {
        uint64_t word_idx = pos / 64ULL;
        uint64_t word_offset = pos % 64;
        uint64_t block = pos / kNbBitsPerRankSample;
        uint64_t r = ranks_[block];
        for (uint64_t w = block * kNbBitsPerRankSample / 64; w < word_idx; ++w) {
            r += popcount_64(bitArray_[w]);
        }
        uint64_t mask = (uint64_t(1) << word_offset) - 1;
        r += popcount_64(bitArray_[word_idx] & mask);
        return r;
    }

    void save(std::ostream& os) const {
        os.write(reinterpret_cast<const char*>(&size_), sizeof(size_));
        os.write(reinterpret_cast<const char*>(&nchar_), sizeof(nchar_));
        os.write(reinterpret_cast<const char*>(bitArray_),
                 static_cast<std::streamsize>(sizeof(uint64_t) * nchar_));
        size_t sizer = ranks_.size();
        os.write(reinterpret_cast<const char*>(&sizer), sizeof(size_t));
        os.write(reinterpret_cast<const char*>(ranks_.data()),
                 static_cast<std::streamsize>(sizeof(ranks_[0]) * ranks_.size()));
    }

    void load(std::istream& is) {
        is.read(reinterpret_cast<char*>(&size_), sizeof(size_));
        is.read(reinterpret_cast<char*>(&nchar_), sizeof(nchar_));
        this->resize(size_);
        is.read(reinterpret_cast<char*>(bitArray_),
                static_cast<std::streamsize>(sizeof(uint64_t) * nchar_));

        size_t sizer;
        is.read(reinterpret_cast<char*>(&sizer), sizeof(size_t));
        ranks_.resize(sizer);
        is.read(reinterpret_cast<char*>(ranks_.data()),
                static_cast<std::streamsize>(sizeof(ranks_[0]) * ranks_.size()));
    }

private:
    uint64_t* bitArray_;
    uint64_t size_;
    uint64_t nchar_;
    static constexpr uint64_t kNbBitsPerRankSample = 512;
    std::vector<uint64_t> ranks_;
};

// =============================================================================
// Level Structure for MPHF
// =============================================================================

class level {
public:
    level() = default;
    ~level() = default;

    uint64_t get(uint64_t hash_raw) {
        uint64_t hashi = fastrange64(hash_raw, hash_domain);
        return bitset.get(hashi);
    }

    uint64_t idx_begin;
    uint64_t hash_domain;
    bitVector bitset;
};

// =============================================================================
// Iterator Range Helper
// =============================================================================

template <typename Iterator>
struct iter_range {
    iter_range(Iterator b, Iterator e) : m_begin(b), m_end(e) {}
    Iterator begin() const { return m_begin; }
    Iterator end() const { return m_end; }
    Iterator m_begin, m_end;
};

template <typename Iterator>
iter_range<Iterator> range(Iterator begin, Iterator end) {
    return iter_range<Iterator>(begin, end);
}

// =============================================================================
// Minimal Perfect Hash Function (MPHF)
// =============================================================================

/**
 * @brief Minimal Perfect Hash Function using BBHash algorithm
 * @tparam elem_t Type of elements to hash
 * @tparam Hasher_t Hash functor type
 */
template <typename elem_t, typename Hasher_t>
class mphf {
    using MultiHasher_t = XorshiftHashFunctors<elem_t, Hasher_t>;

public:
    mphf() : built_(false), nelem_(0), nb_levels_(0), lastbitsetrank_(0) {}

    ~mphf() = default;

    /**
     * @brief Construct MPHF from input range
     * @param n Number of elements
     * @param input_range Range of input elements
     * @param num_thread Number of threads for construction
     * @param gamma Space-time tradeoff parameter (default 2.0)
     * @param writeEach Write each level to disk (for large datasets)
     * @param progress Show progress bar
     * @param perc_elem_loaded Percentage of elements to load in RAM
     */
    template <typename Range>
    mphf(size_t n, Range const& input_range, int num_thread = 1,
         double gamma = 2.0, bool writeEach = true, bool progress = true,
         float perc_elem_loaded = 0.03);

    /**
     * @brief Lookup element in MPHF
     * @param elem Element to lookup
     * @return Minimal perfect hash value, or ULLONG_MAX if not found
     */
    uint64_t lookup(elem_t elem) {
        if (!built_) return ULLONG_MAX;

        hash_pair_t bbhash = {0, 0};
        int level;
        uint64_t level_hash = getLevel(bbhash, elem, &level);

        if (level == nb_levels_ - 1) {
            auto in_final_map = final_hash_.find(elem);
            if (in_final_map == final_hash_.end()) {
                return ULLONG_MAX;
            }
            return in_final_map->second + lastbitsetrank_;
        }

        uint64_t non_minimal_hp = fastrange64(level_hash, levels_[level].hash_domain);
        return levels_[level].bitset.rank(non_minimal_hp);
    }

    uint64_t nbKeys() const { return nelem_; }

    uint64_t totalBitSize() {
        uint64_t totalsizeBitset = 0;
        for (int ii = 0; ii < nb_levels_; ii++) {
            totalsizeBitset += levels_[ii].bitset.bitSize();
        }
        return totalsizeBitset + final_hash_.size() * 42 * 8;
    }

private:
    uint64_t getLevel(hash_pair_t& bbhash, elem_t elem, int* level,
                      int maxLevel = 100, int minLevel = -1) {
        uint64_t level_hash = hasher_.h0(bbhash, elem);
        
        for (int ii = 0; ii < nb_levels_ && ii <= maxLevel; ii++) {
            if (ii > minLevel) {
                uint64_t hashi = fastrange64(level_hash, levels_[ii].hash_domain);
                if (levels_[ii].bitset.get(hashi)) {
                    *level = ii;
                    return level_hash;
                }
            }
            
            if (ii == 0) {
                level_hash = hasher_.h1(bbhash, elem);
            } else {
                level_hash = hasher_.next(bbhash);
            }
        }
        
        *level = nb_levels_ - 1;
        return level_hash;
    }

    bool built_;
    double gamma_;
    uint64_t hash_domain_;
    uint64_t nelem_;
    int num_thread_;
    int nb_levels_;
    uint64_t lastbitsetrank_;
    
    std::vector<level> levels_;
    std::unordered_map<elem_t, uint64_t> final_hash_;
    MultiHasher_t hasher_;
};

}  // namespace boomphf

// Type aliases for Spring usage
using hasher_t = boomphf::SingleHashFunctor<uint64_t>;
using boophf_t = boomphf::mphf<uint64_t, hasher_t>;

}  // namespace spring

#endif  // SPRING_CORE_BOOPHF_H_
