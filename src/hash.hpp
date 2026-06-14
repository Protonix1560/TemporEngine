
#ifndef HASH_HPP_
#define HASH_HPP_


#include <iterator>

#include <xxhash.h>



template <typename It>
requires std::input_iterator<It>
inline size_t sequence_hash(It begin, It end) {
    if constexpr (sizeof(size_t) == sizeof(uint64_t)) {
        XXH64_state_t* state = XXH64_createState();
        XXH64_reset(state, 0);
        
        for (auto it = begin; it != end; ++it) {
            const auto& el = *it;
            XXH64_update(state, &el, sizeof(el));
        }
        
        uint64_t hash = XXH64_digest(state);
        XXH64_freeState(state);
        return hash;
    } else if constexpr (sizeof(size_t) == sizeof(uint32_t)) {
        XXH32_state_t* state = XXH32_createState();
        XXH32_reset(state, 0);
        
        for (auto it = begin; it != end; ++it) {
            const auto& el = *it;
            XXH32_update(state, &el, sizeof(el));
        }
        
        uint32_t hash = XXH32_digest(state);
        XXH32_freeState(state);
        return hash;
    } else {
        throw "Unsupported architecture";
    }
}


#endif  // HASH_HPP_
