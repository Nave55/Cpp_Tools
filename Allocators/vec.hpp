#pragma once

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

constexpr size_t DEFAULT_ALIGNMENT = 2 * sizeof(void*);
constexpr size_t BYTE = 1ULL;
constexpr size_t KB = 1024ULL;
constexpr size_t MB = KB * 1024ULL;
constexpr size_t GB = MB * 1024ULL;

constexpr bool isPowerOfTwo(const size_t x) {
  return x != 0 && (x & (x - 1)) == 0;
}

constexpr size_t alignForwardSize(const size_t size, const size_t alignment) {
  return (size + (alignment - 1)) & ~(alignment - 1);
}

constexpr uintptr_t alignForwardUintptr(const uintptr_t p,
                                        const uintptr_t alignment) {
  uintptr_t mask = alignment - 1;
  return (p + mask) & ~mask;
}

[[noreturn]] static inline void panic(const char* msg) {
  std::fprintf(stderr, "PANIC: %s\n", msg);
  std::abort();
}

enum class AllocType {
  Arena,
  Stack,
  Pool,
};

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

  virtual bool supports_resize() const noexcept = 0;

  virtual size_t get_used() const noexcept = 0;

  virtual size_t get_size() const noexcept = 0;

  virtual size_t get_free() const noexcept = 0;

  virtual size_t get_chunk_size() const noexcept = 0;

  virtual AllocType get_type() const noexcept = 0;
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
      ::operator delete[](m_buf, m_buf_len,
                          std::align_val_t{DEFAULT_ALIGNMENT});
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
    if (size <= 0) panic("Size must be greater than zero");
    if (alignment > 128) alignment = 128;

    void* p = m_buf + m_curr_off;
    size_t space = m_buf_len - m_curr_off;

    if (!std::align(alignment, size, p, space))
      panic("Arena can't allocate. Not aligned.");

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
    if (!newp) panic("New Pointer wasn't allocated in arena");

    std::memmove(newp, old_ptr, std::min(old_size, new_size));
    return newp;
  }

  // does nothing don't use.
  void free(void*) noexcept override {
    panic("Arena Can't use free");
  }

  void free_all() noexcept override {
    m_prev_off = m_curr_off = 0;
  }

  template <typename T>
  T* alloc(size_t count = 1, size_t alignment = alignof(T)) noexcept {
    return static_cast<T*>(allocate(sizeof(T) * count, alignment));
  }

  template <typename Old, typename New>
  New* resize_typed(Old* old_mem, size_t old_count = 1, size_t new_count = 1,
                    size_t alignment = alignof(New)) noexcept {
    return static_cast<New*>(resize(old_mem, sizeof(Old) * old_count,
                                    sizeof(New) * new_count, alignment));
  }

  void info() {
    std::printf("Current Offset: %zu\n", m_curr_off);
    std::printf("Previous Offset: %zu\n", m_prev_off);
    std::printf("Size of Arena: %zu\n", m_buf_len);
  }

  bool supports_resize() const noexcept override {
    return true;
  }

  size_t get_used() const noexcept override {
    return m_curr_off;
  }

  size_t get_size() const noexcept override {
    return m_buf_len;
  }

  size_t get_free() const noexcept override {
    return get_size() - get_used();
  }

  size_t get_chunk_size() const noexcept override {
    return m_buf_len;
  }

  AllocType get_type() const noexcept override {
    return AllocType::Arena;
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
  size_t m_curr_off = 0;

public:
  explicit Stack(size_t buf_size = MB) noexcept
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
    if (!isPowerOfTwo(alignment)) panic("Must be a power of two!");

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
    if (!isPowerOfTwo(alignment)) panic("Must be a power of two1");

    if (size <= 0) panic("Size must be greater than zero");
    if (alignment > 128) alignment = 128;

    const uintptr_t base = reinterpret_cast<uintptr_t>(m_buf);
    const uintptr_t curr_addr = base + m_curr_off;

    const size_t header_size = sizeof(StackHeader);
    const size_t padding =
        calc_padding_with_header(curr_addr, alignment, header_size);

    const size_t new_offset = m_curr_off + padding;
    if (new_offset < m_curr_off || new_offset > m_buf_len) return nullptr;

    const size_t end_offset = new_offset + size;
    if (end_offset < new_offset || end_offset > m_buf_len) return nullptr;

    const uintptr_t user_addr = base + new_offset;

    auto header = reinterpret_cast<StackHeader*>(user_addr - header_size);
    if (header->padding > m_buf_len) panic("Padding is > m_buf_len");
    if (header->prev_offset > m_curr_off) panic("prev_off > m_curr_off");

    header->prev_offset = m_curr_off;
    header->padding = padding;
    header->alloc_size = size;

    m_curr_off = end_offset;

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
      panic("Pointer out of bounds (resize)");
      // return nullptr;
    }

    const size_t header_size = sizeof(StackHeader);
    auto* header = reinterpret_cast<StackHeader*>(addr - header_size);
    if (header->padding > m_buf_len) panic("Padding is > m_buf_len");
    if (header->prev_offset > m_curr_off) panic("prev_off > m_curr_off");

    const size_t block_start = header->prev_offset + header->padding;
    const size_t block_size = header->alloc_size;
    const size_t block_end = block_start + block_size;

    // Top-of-stack check
    if (block_end == m_curr_off) {
      // Shrink
      if (new_size <= block_size) {
        m_curr_off = block_start + new_size;
        header->alloc_size = new_size;
        return old_ptr;
      }

      // Grow
      size_t grow = new_size - block_size;
      if (m_curr_off + grow <= m_buf_len) {
        m_curr_off += grow;
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
      panic("Pointer out of bounds (free)");
      // return;
    }

    const size_t header_size = sizeof(StackHeader);
    auto* header = reinterpret_cast<StackHeader*>(addr - header_size);

    const size_t block_start = header->prev_offset + header->padding;
    const size_t block_size = header->alloc_size;

    const uintptr_t expected_addr = base + block_start;
    if (expected_addr != addr) {
      panic("Header mismatch");
      // return;
    }

    // LIFO check
    if (addr + block_size != base + m_curr_off) {
      panic("Out-of-order free");
      // return;
    }

    m_curr_off = header->prev_offset;
  }

  void free_all() noexcept override {
    m_curr_off = 0;
  }

  size_t get_marker() const noexcept {
    return m_curr_off;
  }

  void free_to_marker(size_t marker) noexcept {
    if (marker > m_curr_off) {
      panic("Invalid marker");
      // return;
    }
    m_curr_off = marker;
  }

  void info() {
    std::printf("Current Offset: %zu\n", m_curr_off);
    std::printf("Size of Arena: %zu\n", m_buf_len);
  }

  bool supports_resize() const noexcept override {
    return true;
  }

  size_t get_used() const noexcept override {
    return m_curr_off;
  }

  size_t get_size() const noexcept override {
    return m_buf_len;
  }

  size_t get_free() const noexcept override {
    return get_size() - get_used();
  }

  size_t get_chunk_size() const noexcept override {
    return m_buf_len;
  }

  AllocType get_type() const noexcept override {
    return AllocType::Stack;
  }
};

class TempStack {
private:
  Stack& m_stack;
  size_t m_curr_offset;

public:
  TempStack(Stack& stack)
      : m_stack{stack},
        m_curr_offset{stack.m_curr_off} {}

  ~TempStack() {
    m_stack.m_curr_off = m_curr_offset;
#ifdef DEBUG
    std::printf("Temp Stack Destroyed\n");
#endif
  }

  TempStack(const TempStack&) = delete;
  TempStack& operator=(const TempStack&) = delete;
};

// ********************************************
//                 Pool Allocator
// ********************************************

struct PoolFreeNode {
  PoolFreeNode* next;       // pool free list
  PoolFreeNode* temp_next;  // temp scope tracking
};

class Pool final : public MemAllocator {
private:
  friend class TempPool;
  unsigned char* m_buf = nullptr;
  size_t m_buf_len = 0;
  size_t m_chunk_size = 0;
  PoolFreeNode* m_head = nullptr;

public:
  explicit Pool(size_t buf_size = MB, size_t chunk_size = 64,
                size_t chunk_alignment = alignof(std::max_align_t)) {
    if (buf_size <= 0) panic("Buf size <= 0");
    if (chunk_size <= 0) panic("Chunk size <= 0");
    if (!isPowerOfTwo(chunk_alignment))
      panic("chunk_alignment must be power of two");

    // Allocate the buffer internally (like Arena)
    m_buf_len = buf_size;
    m_buf = static_cast<unsigned char*>(
        ::operator new[](m_buf_len, std::align_val_t{chunk_alignment}));

    // Align chunk size
    m_chunk_size =
        (chunk_size + (chunk_alignment - 1)) & ~(chunk_alignment - 1);

    if (m_chunk_size < sizeof(PoolFreeNode))
      panic("chunk_size too small for free list node");

    // Build free list
    free_all();
  }

  ~Pool() {
    ::operator delete[](m_buf, m_buf_len,
                        std::align_val_t{alignof(std::max_align_t)});
#ifdef DEBUG
    std::printf("Pool Destroyed\n");
#endif
  }

  void* allocate(size_t = 0, size_t = 0) noexcept override {
    if (!m_head) return nullptr;
    PoolFreeNode* node = m_head;
    m_head = node->next;
    return std::memset(node, 0, m_chunk_size);
  }

  template <typename T>
  T* alloc() {
    return static_cast<T*>(allocate());
  }

  void free(void* ptr) noexcept override {
    if (!ptr) return;
    auto* node = static_cast<PoolFreeNode*>(ptr);
    node->next = m_head;
    m_head = node;
  }

  void free_all() noexcept override {
    m_head = nullptr;
    size_t count = m_buf_len / m_chunk_size;
    for (size_t i = 0; i < count; ++i) {
      auto* node = reinterpret_cast<PoolFreeNode*>(m_buf + i * m_chunk_size);
      node->next = m_head;
      m_head = node;
    }
  }

  // Pools do not support resize
  void* resize(void*, size_t, size_t, size_t) noexcept override {
    panic("Pool Can't resize");
  }

  bool supports_resize() const noexcept override {
    return false;
  }

  size_t get_used() const noexcept override {
    size_t total_chunks = m_buf_len / m_chunk_size;

    size_t free_chunks = 0;
    for (PoolFreeNode* n = m_head; n; n = n->next) free_chunks++;

    size_t used_chunks = total_chunks - free_chunks;
    return used_chunks * m_chunk_size;
  }

  size_t get_size() const noexcept override {
    return m_buf_len;
  }

  size_t get_free() const noexcept override {
    return get_size() - get_used();
  }

  size_t get_chunk_size() const noexcept override {
    return m_chunk_size;
  }

  size_t count_free_nodes() const noexcept {
    size_t n = 0;
    for (PoolFreeNode* p = m_head; p; p = p->next) n++;
    return n;
  }

  AllocType get_type() const noexcept override {
    return AllocType::Pool;
  }
};

class TempPool {
private:
  Pool& m_pool;
  PoolFreeNode* m_temp_head = nullptr;

public:
  explicit TempPool(Pool& p)
      : m_pool(p) {}

  template <typename T>
  T* alloc() {
    void* ptr = m_pool.allocate(sizeof(T), alignof(T));
    auto* node = static_cast<PoolFreeNode*>(ptr);

    node->temp_next = m_temp_head;
    m_temp_head = node;

    return static_cast<T*>(ptr);
  }

  ~TempPool() {
#ifdef DEBUG
    std::printf("Temp Pool Destroyed\n");
#endif
    while (m_temp_head) {
      PoolFreeNode* n = m_temp_head;
      m_temp_head = m_temp_head->temp_next;
      m_pool.free(n);
    }
  }
};

extern Arena arena_alloc;
extern Stack stack_alloc;
extern Pool pool_alloc;
#pragma once

#include <algorithm>
#include <initializer_list>
#include "allocators.hpp"

template <typename T>
void print_value(const T& v);

inline void print_value(int v) {
  printf("%d", v);
}
inline void print_value(size_t v) {
  printf("%zu", v);
}
inline void print_value(float v) {
  printf("%f", v);
}
inline void print_value(double v) {
  printf("%f", v);
}
inline void print_value(const char* s) {
  printf("%s", s);
}
inline void print_value(char c) {
  printf("%c", c);
}

template <typename T>
class Vec {
private:
  MemAllocator* m_alloc;
  T* m_vec{nullptr};
  size_t m_len = 0;
  size_t m_capacity = 0;

public:
  explicit Vec(MemAllocator& alloc)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(sizeof(T) * 10, alignof(T)))},
        m_len{0},
        m_capacity{10} {
    if (sizeof(T) * 10 > m_alloc->get_chunk_size()) {
      panic("Initial Vec capacity does not fit in a pool chunk");
    }
  }

  explicit Vec(MemAllocator& alloc, size_t sz)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(sizeof(T) * sz, alignof(T)))},
        m_len{sz},
        m_capacity{sz} {
    if (sizeof(T) * sz > m_alloc->get_chunk_size())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) m_vec[i] = T();
  }

  explicit Vec(MemAllocator& alloc, size_t sz, size_t cap)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(sizeof(T) * cap, alignof(T)))},
        m_len{sz},
        m_capacity{cap} {
    if (sizeof(T) * cap > m_alloc->get_chunk_size())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) m_vec[i] = T();
  }

  explicit Vec(MemAllocator& alloc, std::initializer_list<T> lst)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(
            m_alloc->allocate(sizeof(T) * lst.size(), alignof(T)))},
        m_len{lst.size()},
        m_capacity{lst.size()} {
    if (sizeof(T) * lst.size() > m_alloc->get_chunk_size())
      panic("Initial Vec capacity does not fit in a pool chunk");

    std::copy(lst.begin(), lst.end(), m_vec);
  }

  explicit Vec(MemAllocator& alloc, std::initializer_list<T> lst, size_t cap)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(
            sizeof(T) * std::max(cap, lst.size()), alignof(T)))},
        m_len{lst.size()},
        m_capacity{std::max(cap, lst.size())} {
    if (sizeof(T) * std::max(cap, lst.size()) > m_alloc->get_chunk_size())
      panic("Initial Vec capacity does not fit in a pool chunk");

    std::copy(lst.begin(), lst.end(), m_vec);
  }

  ~Vec() {}

  Vec(const Vec& vec)
      : m_alloc{vec.m_alloc},
        m_vec{m_alloc->allocate(sizeof(T) * vec.m_capacity, alignof(T))},
        m_len{vec.m_len},
        m_capacity{vec.m_capacity} {
    std::copy(vec.m_vec, vec.m_vec + vec.m_len, m_vec);
  }

  Vec(Vec&& vec)
      : m_alloc{vec.m_alloc},
        m_vec{std::move(vec.m_vec)},
        m_len{vec.m_len},
        m_capacity{vec.m_capacity} {
    vec.m_vec = nullptr;
    vec.m_len = 0;
    vec.m_capacity = 0;
  }

  Vec& operator=(const Vec&) = delete;
  Vec& operator=(Vec&&) = delete;

  T* data() {
    return m_vec;
  }

  T& operator[](size_t i) noexcept {
    if (i >= m_len) panic("Vec::operator[] out of bounds");
    return m_vec[i];
  }

  const T& operator[](size_t i) const noexcept {
    if (i >= m_len) panic("Vec::operator[] out of bounds");
    return m_vec[i];
  }

  size_t capacity() const noexcept {
    return m_capacity;
  }

  size_t size() const noexcept {
    return m_len;
  }

  T* begin() const noexcept {
    return m_vec;
  }

  T* end() const noexcept {
    return m_vec + m_len;
  }

  const char* type() const noexcept {
    return typeid(T).name();
  }

  void print() const noexcept {
    if (m_len == 0) {
      std::printf("[]\n");
      return;
    }
    for (size_t i = 0; i < m_len; i++) {
      if (i == 0) {
        std::printf("[");
        print_value(m_vec[i]);
      } else if (i < m_len - 1) {
        std::printf(", ");
        print_value(m_vec[i]);
      } else {
        std::printf(", ");
        print_value(m_vec[i]);
        std::printf("]\n");
      }

      if (m_len == 1) std::printf("]\n");
    }
  }

  void printInfo() const noexcept {
    std::printf("length: ");
    print_value(m_len);
    std::printf(", capacity: ");
    print_value(m_capacity);
    std::printf(", type: ");
    print_value(type());
    std::printf("\n");
  }

  T first() const noexcept {
    return m_vec[0];
  }

  T last() const noexcept {
    return m_vec[m_len - 1];
  }

  void clear() noexcept {
    for (size_t i = 0; i < m_len; i++) m_vec[i] = T();
    m_len = 0;
  }

  void fill(const T& val) noexcept {
    std::fill(begin(), end(), val);
  }

  void sort() noexcept {
    std::sort(begin(), end());
  }

  void sortDescending() noexcept {
    std::sort(begin(), end(), [](const T& a, const T& b) { return a > b; });
  }

  template <typename F>
  void sortCustom(F func) noexcept {
    std::sort(begin(), end(), func);
  }

  int linearSearch(T x) const noexcept {
    for (size_t i = 0; i < m_len; i++) {
      if (m_vec[i] == x) return i;
    }

    return -1;
  }

  int binarySearch(T x) const noexcept {
    int high = m_len - 1;
    int low = 0;

    while (low <= high) {
      int mid = low + ((high - low) / 2);
      if (m_vec[mid] == x)
        return mid;
      else if (m_vec[mid] > x)
        high = mid - 1;
      else
        low = mid + 1;
    }

    return -1;
  }

  void extend(size_t new_size, T val = T()) noexcept {
    if (new_size > m_capacity) m_resize_capacity(new_size);
    for (size_t i = m_len; i < new_size; ++i) m_vec[i] = val;
    m_len = new_size;
  }

  void reserve(size_t new_cap) noexcept {
    if (new_cap > m_capacity) m_resize_capacity(new_cap);
  }

  void shrink(size_t new_size) noexcept {
    if (m_len < m_capacity && m_len > 0) m_resize_capacity(new_size);
  }

  void shrinkToFit() noexcept {
    if (m_len < m_capacity) m_resize_capacity(m_len);
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void pushBack(S&& val) noexcept {
    if (m_len == m_capacity) m_resize_capacity(m_capacity * 2);
    m_vec[m_len++] = std::forward<S>(val);
  }

  template <typename... Args>
    requires(sizeof...(Args) == 1 &&
             std::is_same_v<T, std::remove_cvref_t<Args>...>)
  void emplaceBack(Args&&... args) noexcept {
    if (m_len == m_capacity) m_resize_capacity(m_capacity * 2);

    new (&m_vec[m_len]) T(std::forward<Args>(args)...);
    ++m_len;
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void insert(S&& val, size_t ind) noexcept {
    if (ind > m_len) ind = m_len;

    if (m_len == m_capacity) m_resize_capacity(m_capacity * 2);

    for (size_t i = m_len; i > ind; --i) m_vec[i] = m_vec[i - 1];

    m_vec[ind] = std::forward<S>(val);
    ++m_len;
  }

  void pop() noexcept {
    if (m_len <= 0) panic("Vec must be > 0 to pop");
    --m_len;
  }

  T popBack() noexcept {
    if (m_len <= 0) panic("Vec must be > 0 to pop");
    T val = m_vec[m_len - 1];
    --m_len;
    return val;
  }

  void orderedRemove(size_t ind) noexcept {
    if (ind >= m_len) return;

    for (size_t i = ind; i + 1 < m_len; ++i) m_vec[i] = m_vec[i + 1];

    --m_len;
  }

  void unorderedRemove(size_t ind) noexcept {
    if (ind >= m_len) return;

    m_vec[ind] = m_vec[m_len - 1];
    --m_len;
  }

private:
  void m_resize_capacity(size_t new_cap) {
    size_t old_bytes = m_capacity * sizeof(T);
    size_t new_bytes = new_cap * sizeof(T);

    if (m_alloc->supports_resize()) {
      T* new_vec = static_cast<T*>(
          m_alloc->resize(m_vec, old_bytes, new_bytes, alignof(T)));
      if (!new_vec) panic("Allocator resize failed");

      m_vec = new_vec;
      m_capacity = new_cap;
    } else {
      T* new_vec =
          static_cast<T*>(m_alloc->allocate(sizeof(T) * new_cap, alignof(T)));
      if (!new_vec) panic("Allocator resize failed");

      for (size_t i = 0; i < m_len; ++i)
        new (&new_vec[i]) T(std::move(m_vec[i]));

      for (size_t i = 0; i < m_len; ++i) m_vec[i].~T();

      m_alloc->free(m_vec);
      m_vec = new_vec;
      m_capacity = new_cap;
    }
  }
};
#pragma once

#include <algorithm>
#include <initializer_list>
#include "allocators.hpp"

template <typename T>
void print_value(const T& v);

inline void print_value(int v) {
  printf("%d", v);
}
inline void print_value(size_t v) {
  printf("%zu", v);
}
inline void print_value(float v) {
  printf("%f", v);
}
inline void print_value(double v) {
  printf("%f", v);
}
inline void print_value(const char* s) {
  printf("%s", s);
}
inline void print_value(char c) {
  printf("%c", c);
}

template <typename T>
class Vec {
private:
  MemAllocator* m_alloc;
  T* m_vec{nullptr};
  size_t m_len = 0;
  size_t m_capacity = 0;

public:
  explicit Vec(MemAllocator& alloc)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(sizeof(T) * 10, alignof(T)))},
        m_len{0},
        m_capacity{10} {
    if (sizeof(T) * 10 > m_alloc->get_chunk_size()) {
      panic("Initial Vec capacity does not fit in a pool chunk");
    }
  }

  explicit Vec(MemAllocator& alloc, size_t sz)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(sizeof(T) * sz, alignof(T)))},
        m_len{sz},
        m_capacity{sz} {
    if (sizeof(T) * sz > m_alloc->get_chunk_size())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) m_vec[i] = T();
  }

  explicit Vec(MemAllocator& alloc, size_t sz, size_t cap)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(sizeof(T) * cap, alignof(T)))},
        m_len{sz},
        m_capacity{cap} {
    if (sizeof(T) * cap > m_alloc->get_chunk_size())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) m_vec[i] = T();
  }

  explicit Vec(MemAllocator& alloc, std::initializer_list<T> lst)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(
            m_alloc->allocate(sizeof(T) * lst.size(), alignof(T)))},
        m_len{lst.size()},
        m_capacity{lst.size()} {
    if (sizeof(T) * lst.size() > m_alloc->get_chunk_size())
      panic("Initial Vec capacity does not fit in a pool chunk");

    std::copy(lst.begin(), lst.end(), m_vec);
  }

  explicit Vec(MemAllocator& alloc, std::initializer_list<T> lst, size_t cap)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(
            sizeof(T) * std::max(cap, lst.size()), alignof(T)))},
        m_len{lst.size()},
        m_capacity{std::max(cap, lst.size())} {
    if (sizeof(T) * std::max(cap, lst.size()) > m_alloc->get_chunk_size())
      panic("Initial Vec capacity does not fit in a pool chunk");

    std::copy(lst.begin(), lst.end(), m_vec);
  }

  ~Vec() {}

  Vec(const Vec& vec)
      : m_alloc{vec.m_alloc},
        m_vec{m_alloc->allocate(sizeof(T) * vec.m_capacity, alignof(T))},
        m_len{vec.m_len},
        m_capacity{vec.m_capacity} {
    std::copy(vec.m_vec, vec.m_vec + vec.m_len, m_vec);
  }

  Vec(Vec&& vec)
      : m_alloc{vec.m_alloc},
        m_vec{std::move(vec.m_vec)},
        m_len{vec.m_len},
        m_capacity{vec.m_capacity} {
    vec.m_vec = nullptr;
    vec.m_len = 0;
    vec.m_capacity = 0;
  }

  Vec& operator=(const Vec&) = delete;
  Vec& operator=(Vec&&) = delete;

  T* data() {
    return m_vec;
  }

  T& operator[](size_t i) noexcept {
    if (i >= m_len) panic("Vec::operator[] out of bounds");
    return m_vec[i];
  }

  const T& operator[](size_t i) const noexcept {
    if (i >= m_len) panic("Vec::operator[] out of bounds");
    return m_vec[i];
  }

  size_t capacity() const noexcept {
    return m_capacity;
  }

  size_t size() const noexcept {
    return m_len;
  }

  T* begin() const noexcept {
    return m_vec;
  }

  T* end() const noexcept {
    return m_vec + m_len;
  }

  const char* type() const noexcept {
    return typeid(T).name();
  }

  void print() const noexcept {
    if (m_len == 0) {
      std::printf("[]\n");
      return;
    }
    for (size_t i = 0; i < m_len; i++) {
      if (i == 0) {
        std::printf("[");
        print_value(m_vec[i]);
      } else if (i < m_len - 1) {
        std::printf(", ");
        print_value(m_vec[i]);
      } else {
        std::printf(", ");
        print_value(m_vec[i]);
        std::printf("]\n");
      }

      if (m_len == 1) std::printf("]\n");
    }
  }

  void printInfo() const noexcept {
    std::printf("length: ");
    print_value(m_len);
    std::printf(", capacity: ");
    print_value(m_capacity);
    std::printf(", type: ");
    print_value(type());
    std::printf("\n");
  }

  T first() const noexcept {
    return m_vec[0];
  }

  T last() const noexcept {
    return m_vec[m_len - 1];
  }

  void clear() noexcept {
    for (size_t i = 0; i < m_len; i++) m_vec[i] = T();
    m_len = 0;
  }

  void fill(const T& val) noexcept {
    std::fill(begin(), end(), val);
  }

  void sort() noexcept {
    std::sort(begin(), end());
  }

  void sortDescending() noexcept {
    std::sort(begin(), end(), [](const T& a, const T& b) { return a > b; });
  }

  int linearSearch(T x) const noexcept {
    for (size_t i = 0; i < m_len; i++) {
      if (m_vec[i] == x) return i;
    }

    return -1;
  }

  int binarySearch(T x) const noexcept {
    int high = m_len - 1;
    int low = 0;

    while (low <= high) {
      int mid = low + ((high - low) / 2);
      if (m_vec[mid] == x)
        return mid;
      else if (m_vec[mid] > x)
        high = mid - 1;
      else
        low = mid + 1;
    }

    return -1;
  }

  void extend(size_t new_size, T val = T()) noexcept {
    if (new_size > m_capacity) m_resize_capacity(new_size);
    for (size_t i = m_len; i < new_size; ++i) m_vec[i] = val;
    m_len = new_size;
  }

  void reserve(size_t new_cap) noexcept {
    if (new_cap > m_capacity) m_resize_capacity(new_cap);
  }

  void shrink(size_t new_size) noexcept {
    if (m_len < m_capacity && m_len > 0) m_resize_capacity(new_size);
  }

  void shrinkToFit() noexcept {
    if (m_len < m_capacity) m_resize_capacity(m_len);
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void pushBack(S&& val) noexcept {
    if (m_len == m_capacity) m_resize_capacity(m_capacity * 2);
    m_vec[m_len++] = std::forward<S>(val);
  }

  template <typename... Args>
    requires(sizeof...(Args) == 1 &&
             std::is_same_v<T, std::remove_cvref_t<Args>...>)
  void emplaceBack(Args&&... args) noexcept {
    if (m_len == m_capacity) m_resize_capacity(m_capacity * 2);

    new (&m_vec[m_len]) T(std::forward<Args>(args)...);
    ++m_len;
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void insert(S&& val, size_t ind) noexcept {
    if (ind > m_len) ind = m_len;

    if (m_len == m_capacity) m_resize_capacity(m_capacity * 2);

    for (size_t i = m_len; i > ind; --i) m_vec[i] = m_vec[i - 1];

    m_vec[ind] = std::forward<S>(val);
    ++m_len;
  }

  void pop() noexcept {
    if (m_len <= 0) panic("Vec must be > 0 to pop");
    --m_len;
  }

  T popBack() noexcept {
    if (m_len <= 0) panic("Vec must be > 0 to pop");
    T val = m_vec[m_len - 1];
    --m_len;
    return val;
  }

  void orderedRemove(size_t ind) noexcept {
    if (ind >= m_len) return;

    for (size_t i = ind; i + 1 < m_len; ++i) m_vec[i] = m_vec[i + 1];

    --m_len;
  }

  void unorderedRemove(size_t ind) noexcept {
    if (ind >= m_len) return;

    m_vec[ind] = m_vec[m_len - 1];
    --m_len;
  }

private:
  void m_resize_capacity(size_t new_cap) {
    size_t old_bytes = m_capacity * sizeof(T);
    size_t new_bytes = new_cap * sizeof(T);

    if (m_alloc->supports_resize()) {
      T* new_vec = static_cast<T*>(
          m_alloc->resize(m_vec, old_bytes, new_bytes, alignof(T)));
      if (!new_vec) panic("Allocator resize failed");

      m_vec = new_vec;
      m_capacity = new_cap;
    } else {
      T* new_vec =
          static_cast<T*>(m_alloc->allocate(sizeof(T) * new_cap, alignof(T)));
      if (!new_vec) panic("Allocator resize failed");

      for (size_t i = 0; i < m_len; ++i)
        new (&new_vec[i]) T(std::move(m_vec[i]));

      for (size_t i = 0; i < m_len; ++i) m_vec[i].~T();

      m_alloc->free(m_vec);
      m_vec = new_vec;
      m_capacity = new_cap;
    }
  }
};
