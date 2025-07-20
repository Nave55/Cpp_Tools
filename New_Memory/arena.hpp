#pragma once

#include <new>         // for operator new/delete
#include <cassert>
#include <cstddef>
#include <cstring>
#include <algorithm>   // for std::min
#include <memory>      // for std::align
#include <iostream>

constexpr size_t DEFAULT_ALIGNMENT = 2 * sizeof(void*);
constexpr size_t KB =                1024ULL;
constexpr size_t MB =                KB * 1024ULL;
constexpr size_t GB =                MB * 1024ULL;

/**
 * \brief Returns true if the given size_t is a power of two.
 *
 * A power of two is any number that can be written as 2^n, where n is a
 * non-negative integer. This function is useful for checking if a given size
 * is a power of two, since it can be an important property for memory
 * allocation and other performance-critical code.
 *
 * \param[in] x The size_t to check.
 *
 * \return True if the given size_t is a power of two, false otherwise.
 */
constexpr auto is_power_of_two(const size_t x) -> bool {
  return x != 0 && (x & (x - 1)) == 0;
}

class Arena {
private:
    friend class TempArena;
    unsigned char* m_buf       = nullptr;
    size_t         m_buf_len   = 0;
    size_t         m_prev_off  = 0;
    size_t         m_curr_off  = 0;

public:
    Arena(size_t buf_size = MB)
    : m_buf_len(buf_size) {
        m_buf = static_cast<unsigned char*>(
            ::operator new[](m_buf_len, std::align_val_t{DEFAULT_ALIGNMENT})
        );
    }

    ~Arena() {
        ::operator delete[](m_buf, std::align_val_t{DEFAULT_ALIGNMENT});
        std::cout << "Arena destroyed\n";
    }

    Arena(const Arena&)            = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&& o) noexcept
        : m_buf(o.m_buf), m_buf_len(o.m_buf_len)
        , m_prev_off(o.m_prev_off), m_curr_off(o.m_curr_off) {
            o.m_buf = nullptr;
            o.m_buf_len = 0;
        }
        
    Arena& operator=(Arena&& o) noexcept {
        if (this != &o) {
            ::operator delete[](m_buf, std::align_val_t{DEFAULT_ALIGNMENT});
            m_buf = o.m_buf; m_buf_len = o.m_buf_len;
            m_prev_off = o.m_prev_off; m_curr_off = o.m_curr_off;
            o.m_buf = nullptr; o.m_buf_len = 0;
        }
        return *this;
    }

    /**
     * Allocate memory block of size count * sizeof(T) in the arena.
     * @param count The number of elements to allocate (default: 1).
     * @param alignment The alignment requirement of the type (default: alignof(T)).
     * @return A pointer to the allocated memory or nullptr if the request exceeds the arena size.
     * @remark The memory is cleared before returning.
     */
    template<typename T>
    auto alloc(size_t count = 1, size_t alignment = alignof(T)) -> T* {
        size_t bytes = sizeof(T) * count;
        if (bytes == 0) return nullptr;   
                  
        void* p = m_buf + m_curr_off;
        size_t space = m_buf_len - m_curr_off;
        if (!std::align(alignment, bytes, p, space))
            return nullptr;
        m_prev_off = static_cast<unsigned char*>(p) - m_buf;
        m_curr_off = m_prev_off + bytes;
        std::memset(p, 0, bytes);
        return static_cast<T*>(p);
    }

    /**
     * Resize an allocation in the arena from Old to New.
     * @param old_mem The existing allocation to resize (or nullptr to allocate a new block).
     * @param old_count The number of elements in the existing allocation (default: 1).
     * @param new_count The number of elements to allocate for the new block (default: 1).
     * @param alignment The alignment requirement of the new block (default: alignof(New)).
     * @return A pointer to the resized block or nullptr if the request exceeds the arena size.
     * @remark The memory is cleared before returning. If the new size is smaller than the old size,
     * the trailing bytes of the old block are left untouched.
     */
    template<typename Old, typename New>
    auto resize(Old* old_mem, size_t old_count = 1, size_t new_count = 1, size_t alignment = alignof(New)) -> New* {
        size_t old_bytes = sizeof(Old) * old_count;
        size_t new_bytes = sizeof(New) * new_count;

        if (!old_mem || old_count == 0)
            return alloc<New>(new_count, alignment);

        auto p   = reinterpret_cast<unsigned char*>(old_mem);
        size_t off = p - m_buf;

        // in‑place if it’s the last alloc and fits
        if (off == m_prev_off &&
            m_curr_off - m_prev_off + (new_bytes - old_bytes) <= (m_buf_len - m_prev_off))
        {
            m_curr_off = m_prev_off + new_bytes;
            if (new_bytes > old_bytes)
                std::memset(m_buf + m_curr_off - (new_bytes - old_bytes),
                            0, new_bytes - old_bytes);
            return reinterpret_cast<New*>(p);
        }

        // otherwise bump‑allocate fresh
        New* newp = alloc<New>(new_count, alignment);
        if (!newp) return nullptr;
        // copy the smaller of old_bytes/new_bytes
        std::memmove(newp, old_mem, std::min(old_bytes, new_bytes));
        return newp;
    }

    auto free_all() noexcept -> void {
        m_prev_off = m_curr_off = 0;
    }

};

// Arena for temporary allocations. Raii resets arena on scope exit to original arena.
class TempArena {
private:
    Arena& arena;
    size_t m_prev_off, m_curr_off;

public:
    TempArena(Arena& a)
        : arena(a)
        , m_prev_off(a.m_prev_off)
        , m_curr_off(a.m_curr_off)
        {}

    ~TempArena() {
        arena.m_prev_off = m_prev_off;
        arena.m_curr_off = m_curr_off;
    }

    TempArena(const TempArena&)            = delete;
    TempArena& operator=(const TempArena&) = delete;
};
