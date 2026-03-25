#pragma once

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

constexpr size_t DEFAULT_ALIGNMENT = 2 * sizeof(void*);
constexpr size_t KB = 1024ULL;
constexpr size_t MB = KB * 1024ULL;
constexpr size_t GB = MB * 1024ULL;

constexpr auto isPowerOfTwo(const size_t x) -> bool {
  return x != 0 && (x & (x - 1)) == 0;
}

// *******************************************************
//                Allocator Interface
// *******************************************************

class MemAllocator {
public:
  virtual ~MemAllocator() = default;

  virtual void* allocate(size_t bytes, size_t alignment) noexcept = 0;

  virtual void* resize(void* old_ptr, size_t old_size, size_t new_size,
                       size_t alignment) noexcept = 0;

  virtual void free(void* ptr) noexcept = 0;
  virtual void free_all() noexcept = 0;
};

// *******************************************************
//                   Arena Allocator
// *******************************************************
class Arena final : public MemAllocator {
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
    ::operator delete[](m_buf, m_buf_len, std::align_val_t{DEFAULT_ALIGNMENT});
#ifdef DEBUG
    std::printf("Arena destroyed\n");
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

  void* allocate(size_t size, size_t alignment) noexcept override {
    assert(size > 0 && "Bytes must be greater than zero");
    if (alignment > 128) alignment = 128;

    void* p = m_buf + m_curr_off;
    size_t space = m_buf_len - m_curr_off;

    if (!std::align(alignment, size, p, space)) return nullptr;

    m_prev_off = static_cast<unsigned char*>(p) - m_buf;
    m_curr_off = m_prev_off + size;

    std::memset(p, 0, size);
    return p;
  }

  void* resize(void* old_ptr, size_t old_size, size_t new_size,
               size_t alignment) noexcept override {
    if (!old_ptr || old_size == 0) return allocate(new_size, alignment);

    unsigned char* p = static_cast<unsigned char*>(old_ptr);
    size_t off = p - m_buf;

    // In-place resize if last allocation
    if (off == m_prev_off && m_curr_off - m_prev_off + (new_size - old_size) <=
                                 (m_buf_len - m_prev_off)) {
      m_curr_off = m_prev_off + new_size;

      if (new_size > old_size) {
        std::memset(m_buf + m_curr_off - (new_size - old_size), 0,
                    new_size - old_size);
      }

      return p;
    }

    // Allocate fresh
    void* newp = allocate(new_size, alignment);
    if (!newp) return nullptr;

    std::memmove(newp, old_ptr, std::min(old_size, new_size));
    return newp;
  }

  // does nothing don't use.
  void free(void*) noexcept override {}

  void free_all() noexcept override {
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
    std::printf("Current Offset: %zu\n", m_curr_off);
    std::printf("Previous Offset: %zu\n", m_prev_off);
    std::printf("Size of Arena: %zu\n", m_buf_len);
  }
};

// TempArena (checkpoint / rollback)
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
    std::printf("Temp Arena Destroyed\n");
#endif
  }

  TempArena(const TempArena&) = delete;
  TempArena& operator=(const TempArena&) = delete;
};

// *******************************************************
//                  Stack Allocator
// *******************************************************

// Stack header stored immediately before the returned user pointer
struct StackHeader {
  size_t prev_offset;  // previous top of stack
  size_t padding;      // padding before user pointer
  size_t alloc_size;   // size of allocation
};

class Stack final : public MemAllocator {
private:
  friend class TempStack;
  unsigned char* m_buf = nullptr;
  size_t m_buf_len = 0;
  size_t m_curr_offset = 0;

public:
  Stack(size_t buf_size = MB) noexcept
      : m_buf_len{buf_size} {
    m_buf = static_cast<unsigned char*>(
        ::operator new[](m_buf_len, std::align_val_t{DEFAULT_ALIGNMENT}));
  }

  ~Stack() {
    ::operator delete[](m_buf, m_buf_len, std::align_val_t{DEFAULT_ALIGNMENT});
#ifdef DEBUG
    std::printf("Stack destroyed\n");
#endif
  }

  Stack(const Stack&) = delete;
  Stack& operator=(const Stack&) = delete;

  static constexpr size_t calc_padding_with_header(
      uintptr_t ptr, size_t alignment, size_t header_size) noexcept {
    assert(isPowerOfTwo(alignment));

    const size_t modulo = ptr & (alignment - 1);
    size_t padding = (modulo == 0) ? 0 : (alignment - modulo);

    if (padding < header_size) {
      size_t needed = header_size - padding;
      padding += ((needed + alignment - 1) / alignment) * alignment;
    }

    return padding;
  }

  void* allocate(size_t size,
                 size_t alignment = DEFAULT_ALIGNMENT) noexcept override {
    assert(isPowerOfTwo(alignment));

    assert(size > 0 && "Size must be greater than zero");
    if (alignment > 128) alignment = 128;

    const uintptr_t base = reinterpret_cast<uintptr_t>(m_buf);
    const uintptr_t curr_addr = base + m_curr_offset;

    const size_t header_size = sizeof(StackHeader);
    const size_t padding =
        calc_padding_with_header(curr_addr, alignment, header_size);

    const size_t new_offset = m_curr_offset + padding;
    if (new_offset < m_curr_offset || new_offset > m_buf_len) return nullptr;

    const size_t end_offset = new_offset + size;
    if (end_offset < new_offset || end_offset > m_buf_len) return nullptr;

    const uintptr_t user_addr = base + new_offset;

    auto header = reinterpret_cast<StackHeader*>(user_addr - header_size);
    assert(header->padding <= m_buf_len);
    assert(header->prev_offset <= m_curr_offset);

    header->prev_offset = m_curr_offset;
    header->padding = padding;
    header->alloc_size = size;

    m_curr_offset = end_offset;

    // Zero memory (optional)
    return std::memset(reinterpret_cast<void*>(user_addr), 0, size);
  }

  template <typename T>
  T* alloc(size_t count = 1, size_t alignment = alignof(T)) noexcept {
    static_assert(!std::is_abstract<T>::value, "alloc of abstract type");

    if (count > SIZE_MAX / sizeof(T)) return nullptr;

    void* p = allocate(sizeof(T) * count, alignment);
    return static_cast<T*>(p);
  }

  void* resize(void* old_ptr, size_t /*old_size*/, size_t new_size,
               size_t alignment = DEFAULT_ALIGNMENT) noexcept override {
    if (!old_ptr) return allocate(new_size, alignment);
    if (new_size == 0) {
      free(old_ptr);
      return nullptr;
    }

    const uintptr_t base = reinterpret_cast<uintptr_t>(m_buf);
    const uintptr_t addr = reinterpret_cast<uintptr_t>(old_ptr);

    if (!(base <= addr && addr < base + m_buf_len)) {
      assert(false && "Pointer out of bounds (resize)");
      return nullptr;
    }

    const size_t header_size = sizeof(StackHeader);
    auto* header = reinterpret_cast<StackHeader*>(addr - header_size);
    assert(header->padding <= m_buf_len);
    assert(header->prev_offset <= m_curr_offset);

    const size_t block_start = header->prev_offset + header->padding;
    const size_t block_size = header->alloc_size;
    const size_t block_end = block_start + block_size;

    // Top-of-stack check
    if (block_end == m_curr_offset) {
      // Shrink
      if (new_size <= block_size) {
        m_curr_offset = block_start + new_size;
        header->alloc_size = new_size;
        return old_ptr;
      }

      // Grow
      size_t grow = new_size - block_size;
      if (m_curr_offset + grow <= m_buf_len) {
        m_curr_offset += grow;
        header->alloc_size = new_size;
        return old_ptr;
      }
    }

    // Fallback
    void* new_ptr = allocate(new_size, alignment);
    if (!new_ptr) return nullptr;

    size_t copy_size = std::min(header->alloc_size, new_size);
    std::memmove(new_ptr, old_ptr, copy_size);

    return new_ptr;
  }

  void free(void* ptr) noexcept override {
    if (!ptr) return;

    const uintptr_t base = reinterpret_cast<uintptr_t>(m_buf);
    const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

    if (!(base <= addr && addr < base + m_buf_len)) {
      assert(false && "Pointer out of bounds (free)");
      return;
    }

    const size_t header_size = sizeof(StackHeader);
    auto* header = reinterpret_cast<StackHeader*>(addr - header_size);

    const size_t block_start = header->prev_offset + header->padding;
    const size_t block_size = header->alloc_size;

    const uintptr_t expected_addr = base + block_start;
    if (expected_addr != addr) {
      assert(false && "Header mismatch");
      return;
    }

    // LIFO check
    if (addr + block_size != base + m_curr_offset) {
      assert(false && "Out-of-order free");
      return;
    }

    m_curr_offset = header->prev_offset;
  }

  void free_all() noexcept override {
    m_curr_offset = 0;
  }

  size_t get_marker() const noexcept {
    return m_curr_offset;
  }

  void free_to_marker(size_t marker) noexcept {
    if (marker > m_curr_offset) {
      assert(false && "Invalid marker");
      return;
    }
    m_curr_offset = marker;
  }
};

class TempStack {
private:
  Stack& m_stack;
  size_t m_curr_offset;

public:
  TempStack(Stack& stack)
      : m_stack{stack},
        m_curr_offset{stack.m_curr_offset} {}

  ~TempStack() {
    m_stack.m_curr_offset = m_curr_offset;
#ifdef DEBUG
    std::printf("Temp Stack Destroyed\n");
#endif
  }

  TempStack(const TempStack&) = delete;
  TempStack& operator=(const TempStack&) = delete;
};

extern Arena arena;
extern Stack stack;
