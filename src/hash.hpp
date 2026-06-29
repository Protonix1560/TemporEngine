
#ifndef HASH_HPP_
#define HASH_HPP_


#include <iterator>

#include <xxhash.h>


struct xxhash64_holder {
    private:
        XXH64_state_t* mp_state;
    public:
        xxhash64_holder() : mp_state(XXH64_createState()) { if (!mp_state) throw std::bad_alloc{}; }
        ~xxhash64_holder() { XXH64_freeState(mp_state); }
        XXH64_state_t* state() const { return mp_state; }
};


struct xxhash32_holder {
    private:
        XXH32_state_t* mp_state;
    public:
        xxhash32_holder() : mp_state(XXH32_createState()) {}
        ~xxhash32_holder() { XXH32_freeState(mp_state); }
        XXH32_state_t* state() const { return mp_state; }
};


template <typename It>
requires std::input_iterator<It>
inline size_t sequence_hash(It begin, It end) {
    if constexpr (sizeof(size_t) == sizeof(XXH64_hash_t)) {
        thread_local static xxhash64_holder state;
        XXH64_reset(state.state(), 0);
        for (auto it = begin; it != end; ++it) {
            const auto& el = *it;
            XXH64_update(state.state(), &el, sizeof(el));
        }
        return XXH64_digest(state.state());

    } else if constexpr (sizeof(size_t) == sizeof(XXH32_hash_t)) {
        thread_local static xxhash32_holder state;
        XXH32_reset(state.state(), 0);
        for (auto it = begin; it != end; ++it) {
            const auto& el = *it;
            XXH32_update(state.state(), &el, sizeof(el));
        }
        return XXH32_digest(state.state());
        
    } else {
        throw "Unsupported architecture";
    }
}


#endif  // HASH_HPP_
