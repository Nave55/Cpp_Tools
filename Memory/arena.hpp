#pragma once

#include <new>         // for operator new/delete
#include <cassert>
#include <cstddef>
#include <cstring>
#include <algorithm>   // for std::min
#include <memory>      // for std::align

constexpr size_t DEFAULT_ALIGNMENT = 2 * sizeof(void*);
const int KB = 1024;
const int MB = KB * 1024;
const int GB = MB * 1024;

constexpr bool is_power_of_two(size_t x) {
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
    }

    Arena(const Arena&)            = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&& o) noexcept
        : m_buf(o.m_buf), m_buf_len(o.m_buf_len)
        , m_prev_off(o.m_prev_off), m_curr_off(o.m_curr_off) 
        {
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

    auto alloc(size_t size, size_t alignment = DEFAULT_ALIGNMENT) -> void* {
        assert(is_power_of_two(alignment));
        void*  ptr   = m_buf + m_curr_off;
        size_t space = m_buf_len - m_curr_off;
        if (!std::align(alignment, size, ptr, space)) {
            return nullptr;  // or throw std::bad_alloc{};
        }
        m_prev_off = static_cast<unsigned char*>(ptr) - m_buf;
        m_curr_off = m_prev_off + size;
        std::memset(ptr, 0, size);
        return ptr;
    }

    auto resize(void* old_mem, size_t old_size, size_t new_size, size_t alignment = DEFAULT_ALIGNMENT) -> void* {
        if (!old_mem || old_size == 0)
            return alloc(new_size, alignment);

        unsigned char* p    = static_cast<unsigned char*>(old_mem);
        size_t         off  = p - m_buf;
        if (off == m_prev_off &&
            m_curr_off - m_prev_off + (new_size - old_size) <= m_buf_len - m_prev_off) {
            m_curr_off = m_prev_off + new_size;
            if (new_size > old_size)
            std::memset(m_buf + m_curr_off - (new_size - old_size), 0, new_size - old_size);
            return old_mem;
        }

        void* newp = alloc(new_size, alignment);
        if (!newp) return nullptr; // or throw
        std::memmove(newp, old_mem, std::min(old_size, new_size));
        return newp;
    }

    auto free_all() noexcept {
        m_prev_off = m_curr_off = 0;
    }

};

class TempArena {
private:
    Arena& arena;
    size_t m_prev_off, m_curr_off;

public:
    TempArena(Arena& a)
        : arena(a),
        m_prev_off(a.m_prev_off),
        m_curr_off(a.m_curr_off)
    {}

    ~TempArena() {
        arena.m_prev_off = m_prev_off;
        arena.m_curr_off = m_curr_off;
    }

    TempArena(const TempArena&)            = delete;
    TempArena& operator=(const TempArena&) = delete;
};
