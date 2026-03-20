#pragma once

#include <algorithm>  // for std::min
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>  // for std::align
#include <new>     // for operator new/delete

class MemAllocator {
public:
  virtual ~MemAllocator() = default;

  virtual void* allocate(size_t bytes, size_t alignment) = 0;

  virtual void* resize(void* old_ptr, size_t old_bytes, size_t new_bytes,
                       size_t alignment) = 0;

  virtual void free_all() = 0;
};

constexpr size_t DEFAULT_ALIGNMENT = 2 * sizeof(void*);
constexpr size_t KB = 1024ULL;
constexpr size_t MB = KB * 1024ULL;
constexpr size_t GB = MB * 1024ULL;

consteval auto is_power_of_two(const size_t x) -> bool {
  return x != 0 && (x & (x - 1)) == 0;
}

class Arena : public MemAllocator {
private:
  friend class TempArena;

  unsigned char* m_buf = nullptr;
  size_t m_buf_len = 0;
  size_t m_prev_off = 0;
  size_t m_curr_off = 0;

public:
  Arena(size_t buf_size = MB)
      : m_buf_len(buf_size) {
    m_buf = static_cast<unsigned char*>(
        ::operator new[](m_buf_len, std::align_val_t{DEFAULT_ALIGNMENT}));
  }

  ~Arena() {
    ::operator delete[](m_buf, std::align_val_t{DEFAULT_ALIGNMENT});
#ifdef DEBUG
    std::cout << "Arena destroyed\n";
#endif
  }

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  Arena(Arena&& o) noexcept
      : m_buf(o.m_buf),
        m_buf_len(o.m_buf_len),
        m_prev_off(o.m_prev_off),
        m_curr_off(o.m_curr_off) {
    o.m_buf = nullptr;
    o.m_buf_len = 0;
  }

  Arena& operator=(Arena&& o) noexcept {
    if (this != &o) {
      ::operator delete[](m_buf, std::align_val_t{DEFAULT_ALIGNMENT});
      m_buf = o.m_buf;
      m_buf_len = o.m_buf_len;
      m_prev_off = o.m_prev_off;
      m_curr_off = o.m_curr_off;

      o.m_buf = nullptr;
      o.m_buf_len = 0;
    }
    return *this;
  }

  void* allocate(size_t bytes, size_t alignment) override {
    if (bytes == 0) return nullptr;

    void* p = m_buf + m_curr_off;
    size_t space = m_buf_len - m_curr_off;

    if (!std::align(alignment, bytes, p, space)) return nullptr;

    m_prev_off = static_cast<unsigned char*>(p) - m_buf;
    m_curr_off = m_prev_off + bytes;

    std::memset(p, 0, bytes);
    return p;
  }

  void* resize(void* old_ptr, size_t old_bytes, size_t new_bytes,
               size_t alignment) override {
    if (!old_ptr || old_bytes == 0) return allocate(new_bytes, alignment);

    unsigned char* p = static_cast<unsigned char*>(old_ptr);
    size_t off = p - m_buf;

    // In-place resize if last allocation
    if (off == m_prev_off &&
        m_curr_off - m_prev_off + (new_bytes - old_bytes) <=
            (m_buf_len - m_prev_off)) {
      m_curr_off = m_prev_off + new_bytes;

      if (new_bytes > old_bytes) {
        std::memset(m_buf + m_curr_off - (new_bytes - old_bytes), 0,
                    new_bytes - old_bytes);
      }

      return p;
    }

    // Allocate fresh
    void* newp = allocate(new_bytes, alignment);
    if (!newp) return nullptr;

    std::memmove(newp, old_ptr, std::min(old_bytes, new_bytes));
    return newp;
  }

  void free_all() override {
    m_prev_off = m_curr_off = 0;
  }

  template <typename T>
  T* alloc(size_t count = 1, size_t alignment = alignof(T)) {
    return static_cast<T*>(allocate(sizeof(T) * count, alignment));
  }

  template <typename Old, typename New>
  New* resize_typed(Old* old_mem, size_t old_count = 1, size_t new_count = 1,
                    size_t alignment = alignof(New)) {
    return static_cast<New*>(resize(old_mem, sizeof(Old) * old_count,
                                    sizeof(New) * new_count, alignment));
  }

  void info() {
    std::cout << "Current Offset: " << m_curr_off << "\n";
    std::cout << "Previous Offset: " << m_prev_off << "\n";
    std::cout << "Size of Arena: " << m_buf_len << "\n";
  }
};

// ------------------------------------------------------------
// TempArena (checkpoint / rollback)
// ------------------------------------------------------------

class TempArena {
private:
  Arena& arena;
  size_t m_prev_off, m_curr_off;

public:
  TempArena(Arena& a)
      : arena(a),
        m_prev_off(a.m_prev_off),
        m_curr_off(a.m_curr_off) {}

  ~TempArena() {
    arena.m_prev_off = m_prev_off;
    arena.m_curr_off = m_curr_off;
#if DEBUG
    std::cout << "Temp Arena Destroyed\n";
#endif
  }

  TempArena(const TempArena&) = delete;
  TempArena& operator=(const TempArena&) = delete;
};

extern Arena arena;
