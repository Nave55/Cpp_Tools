#pragma once

#include <algorithm>
#include <typeinfo>
#include "allocators.hpp"

template <typename T>
class Vec {
private:
  MemAllocator* m_alloc;
  size_t m_len = 0;
  size_t m_cap = 0;

public:
  T* ptr = nullptr;

public:
  explicit Vec(MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        m_len{0},
        m_cap{8},
        ptr{static_cast<T*>(m_alloc->allocate(sizeof(T) * 8, alignof(T)))} {
    if (sizeof(T) * 10 > m_alloc->getChunkSize()) {
      panic("Initial Vec capacity does not fit in a pool chunk");
    }
  }

  explicit Vec(size_t sz, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        m_len{sz},
        m_cap{sz}, 
        ptr{static_cast<T*>(m_alloc->allocate(sizeof(T) * sz, alignof(T)))} {
    if (sizeof(T) * sz > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) ptr[i] = T();
  }

  explicit Vec(size_t sz, size_t cap, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        m_len{sz},
        m_cap{cap}, 
        ptr{static_cast<T*>(m_alloc->allocate(sizeof(T) * cap, alignof(T)))} {
    if (sizeof(T) * cap > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) ptr[i] = T();
  }

  explicit Vec(std::initializer_list<T> lst, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        m_len{lst.size()},
        m_cap{lst.size()}, 
        ptr{static_cast<T*>(
            m_alloc->allocate(sizeof(T) * lst.size(), alignof(T)))} {
    if (sizeof(T) * lst.size() > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    std::copy(lst.begin(), lst.end(), ptr);
  }

  explicit Vec(std::initializer_list<T> lst, size_t cap,
               MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        m_len{lst.size()},
        m_cap{std::max(cap, lst.size())},
        ptr{static_cast<T*>(m_alloc->allocate(
            sizeof(T) * std::max(cap, lst.size()), alignof(T)))} {
    if (sizeof(T) * std::max(cap, lst.size()) > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    std::copy(lst.begin(), lst.end(), ptr);
  }

  ~Vec() {
    if (m_alloc->getType() == AllocType::Pool) m_alloc->free(ptr);
  }

  Vec(const Vec& vec)
      : m_alloc{vec.m_alloc},
        ptr{m_alloc->allocate(sizeof(T) * vec.m_cap, alignof(T))},
        m_len{vec.m_len},
        m_cap{vec.m_cap} {
    std::copy(vec.ptr, vec.ptr + vec.m_len, ptr);
  }

  Vec(Vec&& vec)
      : m_alloc{vec.m_alloc},
        ptr{std::move(vec.ptr)},
        m_len{vec.m_len},
        m_cap{vec.m_cap} {
    vec.ptr = nullptr;
    vec.m_len = 0;
    vec.m_cap = 0;
  }

  Vec& operator=(const Vec& other) {
    if (this == &other) return *this;

    if (m_alloc->getType() == AllocType::Pool) {
      if (ptr) {
        m_alloc->free(ptr);
      }
    }

    m_alloc = other.m_alloc;

    ptr = static_cast<T*>(
        m_alloc->allocate(sizeof(T) * other.m_cap, alignof(T)));
    if (!ptr) panic("Vec copy assignment: allocation failed");

    m_len = other.m_len;
    m_cap = other.m_cap;

    std::copy(other.ptr, other.ptr + other.m_len, ptr);

    return *this;
  }

  Vec& operator=(Vec&& other) noexcept {
    if (this == &other) return *this;

    if (ptr && m_alloc && m_alloc->getType() == AllocType::Pool) {
      m_alloc->free(ptr);
    }

    m_alloc = other.m_alloc;
    ptr = other.ptr;
    m_len = other.m_len;
    m_cap = other.m_cap;

    other.ptr = nullptr;
    other.m_len = 0;
    other.m_cap = 0;

    return *this;
  }

  // T* data() {
  //   return ptr;
  // }

  T& operator[](size_t i) noexcept {
    if (i >= m_len) panic("Vec::operator[] out of bounds");
    return ptr[i];
  }

  const T& operator[](size_t i) const noexcept {
    if (i >= m_len) panic("Vec::operator[] out of bounds");
    return ptr[i];
  }

  size_t capacity() const noexcept {
    return m_cap;
  }

  size_t size() const noexcept {
    return m_len;
  }

  T* begin() const noexcept {
    return ptr;
  }

  T* end() const noexcept {
    return ptr + m_len;
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
        m_printValue(ptr[i]);
      } else if (i < m_len - 1) {
        std::printf(", ");
        m_printValue(ptr[i]);
      } else {
        std::printf(", ");
        m_printValue(ptr[i]);
        std::printf("]\n");
      }

      if (m_len == 1) std::printf("]\n");
    }
  }

  void printInfo() const noexcept {
    std::printf("length: ");
    m_printValue(m_len);
    std::printf(", capacity: ");
    m_printValue(m_cap);
    std::printf(", type: ");
    m_printValue(type());
    std::printf("\n");
  }

  T first() const noexcept {
    return ptr[0];
  }

  T last() const noexcept {
    return ptr[m_len - 1];
  }

  void clear() noexcept {
    for (size_t i = 0; i < m_len; i++) ptr[i] = T();
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
      if (ptr[i] == x) return i;
    }

    return -1;
  }

  int binarySearch(T x) const noexcept {
    int high = m_len - 1;
    int low = 0;

    while (low <= high) {
      int mid = low + ((high - low) / 2);
      if (ptr[mid] == x)
        return mid;
      else if (ptr[mid] > x)
        high = mid - 1;
      else
        low = mid + 1;
    }

    return -1;
  }

  void extend(size_t new_size, T val = T()) noexcept {
    if (new_size > m_cap) m_resizeCapacity(new_size);
    for (size_t i = m_len; i < new_size; ++i) ptr[i] = val;
    m_len = new_size;
  }

  void reserve(size_t new_cap) noexcept {
    if (new_cap > m_cap) m_resizeCapacity(new_cap);
  }

  void shrink(size_t new_size) noexcept {
    if (m_len < m_cap && m_len > 0) m_resizeCapacity(new_size);
  }

  void shrinkToFit() noexcept {
    if (m_len < m_cap) m_resizeCapacity(m_len);
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void pushBack(S&& val) noexcept {
    if (m_len == m_cap) m_resizeCapacity(m_cap * 2);
    ptr[m_len++] = std::forward<S>(val);
  }

  template <typename... Args>
    requires(sizeof...(Args) == 1 &&
             std::is_same_v<T, std::remove_cvref_t<Args>...>)
  void emplaceBack(Args&&... args) noexcept {
    if (m_len == m_cap) m_resizeCapacity(m_cap * 2);

    new (&ptr[m_len]) T(std::forward<Args>(args)...);
    ++m_len;
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void insert(S&& val, size_t ind) noexcept {
    if (ind > m_len) ind = m_len;

    if (m_len == m_cap) m_resizeCapacity(m_cap * 2);

    for (size_t i = m_len; i > ind; --i) ptr[i] = ptr[i - 1];

    ptr[ind] = std::forward<S>(val);
    ++m_len;
  }

  void pop() noexcept {
    if (m_len <= 0) panic("Vec must be > 0 to pop");
    --m_len;
  }

  T popBack() noexcept {
    if (m_len <= 0) panic("Vec must be > 0 to pop");
    T val = ptr[m_len - 1];
    --m_len;
    return val;
  }

  void orderedRemove(size_t ind) noexcept {
    if (ind >= m_len) return;

    for (size_t i = ind; i + 1 < m_len; ++i) ptr[i] = ptr[i + 1];

    --m_len;
  }

  void unorderedRemove(size_t ind) noexcept {
    if (ind >= m_len) return;

    ptr[ind] = ptr[m_len - 1];
    --m_len;
  }

  void deleteVal(T val) noexcept {
    for (int i = m_len - 1; i >= 0; --i) {
      if (ptr[i] == val) orderedRemove(i);
    }
  }

  void deleteValUnordered(T val) noexcept {
    for (int i = m_len - 1; i >= 0; --i) {
      if (ptr[i] == val) unorderedRemove(i);
    }
  }

  template <typename F>
  void mapIter(F func) {
    for (size_t i = 0; i < m_len; ++i) {
      func(ptr[i]);
    }
  }

private:
  void m_resizeCapacity(size_t new_cap) {
    size_t old_bytes = m_cap * sizeof(T);
    size_t new_bytes = new_cap * sizeof(T);

    if (m_alloc->supportsResize()) {
      T* new_vec = static_cast<T*>(
          m_alloc->resize(ptr, old_bytes, new_bytes, alignof(T)));
      if (!new_vec) panic("Allocator resize failed");

      ptr = new_vec;
      m_cap = new_cap;
    } else {
      T* new_vec =
          static_cast<T*>(m_alloc->allocate(sizeof(T) * new_cap, alignof(T)));
      if (!new_vec) panic("Allocator resize failed");

      for (size_t i = 0; i < m_len; ++i)
        new (&new_vec[i]) T(std::move(ptr[i]));

      for (size_t i = 0; i < m_len; ++i) ptr[i].~T();

      m_alloc->free(ptr);
      ptr = new_vec;
      m_cap = new_cap;
    }
  }

  void m_printValue(int v) const noexcept {
    printf("%d", v);
  }

  void m_printValue(size_t v) const noexcept {
    printf("%zu", v);
  }

  void m_printValue(float v) const noexcept {
    printf("%f", v);
  }

  void m_printValue(double v) const noexcept {
    printf("%f", v);
  }

  void m_printValue(const char* s) const noexcept {
    printf("%s", s);
  }

  void m_printValue(char c) const noexcept {
    printf("%c", c);
  }
};
