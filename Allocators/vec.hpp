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
    if (sizeof(T) * 10 > m_alloc->getChunkSize()) {
      panic("Initial Vec capacity does not fit in a pool chunk");
    }
  }

  explicit Vec(MemAllocator& alloc, size_t sz)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(sizeof(T) * sz, alignof(T)))},
        m_len{sz},
        m_capacity{sz} {
    if (sizeof(T) * sz > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) m_vec[i] = T();
  }

  explicit Vec(MemAllocator& alloc, size_t sz, size_t cap)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(sizeof(T) * cap, alignof(T)))},
        m_len{sz},
        m_capacity{cap} {
    if (sizeof(T) * cap > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) m_vec[i] = T();
  }

  explicit Vec(MemAllocator& alloc, std::initializer_list<T> lst)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(
            m_alloc->allocate(sizeof(T) * lst.size(), alignof(T)))},
        m_len{lst.size()},
        m_capacity{lst.size()} {
    if (sizeof(T) * lst.size() > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    std::copy(lst.begin(), lst.end(), m_vec);
  }

  explicit Vec(MemAllocator& alloc, std::initializer_list<T> lst, size_t cap)
      : m_alloc{&alloc},
        m_vec{static_cast<T*>(m_alloc->allocate(
            sizeof(T) * std::max(cap, lst.size()), alignof(T)))},
        m_len{lst.size()},
        m_capacity{std::max(cap, lst.size())} {
    if (sizeof(T) * std::max(cap, lst.size()) > m_alloc->getChunkSize())
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
    if (new_size > m_capacity) m_resizeCapacity(new_size);
    for (size_t i = m_len; i < new_size; ++i) m_vec[i] = val;
    m_len = new_size;
  }

  void reserve(size_t new_cap) noexcept {
    if (new_cap > m_capacity) m_resizeCapacity(new_cap);
  }

  void shrink(size_t new_size) noexcept {
    if (m_len < m_capacity && m_len > 0) m_resizeCapacity(new_size);
  }

  void shrinkToFit() noexcept {
    if (m_len < m_capacity) m_resizeCapacity(m_len);
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void pushBack(S&& val) noexcept {
    if (m_len == m_capacity) m_resizeCapacity(m_capacity * 2);
    m_vec[m_len++] = std::forward<S>(val);
  }

  template <typename... Args>
    requires(sizeof...(Args) == 1 &&
             std::is_same_v<T, std::remove_cvref_t<Args>...>)
  void emplaceBack(Args&&... args) noexcept {
    if (m_len == m_capacity) m_resizeCapacity(m_capacity * 2);

    new (&m_vec[m_len]) T(std::forward<Args>(args)...);
    ++m_len;
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void insert(S&& val, size_t ind) noexcept {
    if (ind > m_len) ind = m_len;

    if (m_len == m_capacity) m_resizeCapacity(m_capacity * 2);

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
  void m_resizeCapacity(size_t new_cap) {
    size_t old_bytes = m_capacity * sizeof(T);
    size_t new_bytes = new_cap * sizeof(T);

    if (m_alloc->supportsResize()) {
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
