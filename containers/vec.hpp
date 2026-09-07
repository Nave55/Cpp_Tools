#pragma once

#include <algorithm>
#include <optional>
#include <span>
#include <typeinfo>
#include "allocators.hpp"

template <typename T>
concept Numeric = std::is_arithmetic_v<T> && !std::same_as<T, bool>;

namespace {
inline void printValue(int v) noexcept {
  printf("%d", v);
}

inline void printValue(size_t v) noexcept {
  printf("%zu", v);
}

inline void printValue(float v) noexcept {
  printf("%f", v);
}

inline void printValue(double v) noexcept {
  printf("%f", v);
}

inline void printValue(const char* s) noexcept {
  printf("%s", s);
}

inline void printValue(char c) noexcept {
  printf("%c", c);
}
}  // namespace

template <typename T>
void print(std::span<T> sli) noexcept {
  size_t len = sli.size();
  if (sli.size() == 0) {
    std::printf("[]\n");
    return;
  }
  for (size_t i = 0; i < len; i++) {
    if (i == 0) {
      std::printf("[");
      printValue(sli[i]);
    } else if (i < len - 1) {
      std::printf(", ");
      printValue(sli[i]);
    } else {
      std::printf(", ");
      printValue(sli[i]);
      std::printf("]\n");
    }

    if (len == 1) std::printf("]\n");
  }
}

template <typename T>
class Vec {
private:
  MemAllocator* m_alloc;

public:
  size_t len = 0;
  size_t cap = 0;
  T* ptr = nullptr;

public:
  explicit Vec(MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{0},
        cap{8},
        ptr{static_cast<T*>(m_alloc->allocate(sizeof(T) * 8, alignof(T)))} {
    if (sizeof(T) * 10 > m_alloc->getChunkSize()) {
      panic("Initial Vec capacity does not fit in a pool chunk");
    }
  }

  explicit Vec(size_t sz, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{sz},
        cap{sz},
        ptr{static_cast<T*>(m_alloc->allocate(sizeof(T) * sz, alignof(T)))} {
    if (sizeof(T) * sz > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) ptr[i] = T();
  }

  explicit Vec(size_t sz, size_t cap, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{sz},
        cap{cap},
        ptr{static_cast<T*>(m_alloc->allocate(sizeof(T) * cap, alignof(T)))} {
    if (sizeof(T) * cap > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    for (size_t i = 0; i < sz; ++i) ptr[i] = T();
  }

  explicit Vec(std::initializer_list<T> lst, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{lst.size()},
        cap{lst.size()},
        ptr{static_cast<T*>(
            m_alloc->allocate(sizeof(T) * lst.size(), alignof(T)))} {
    if (sizeof(T) * lst.size() > m_alloc->getChunkSize())
      panic("Initial Vec capacity does not fit in a pool chunk");

    std::copy(lst.begin(), lst.end(), ptr);
  }

  explicit Vec(std::initializer_list<T> lst, size_t cap,
               MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{lst.size()},
        cap{std::max(cap, lst.size())},
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
        ptr{m_alloc->allocate(sizeof(T) * vec.cap, alignof(T))},
        len{vec.len},
        cap{vec.cap} {
    std::copy(vec.ptr, vec.ptr + vec.len, ptr);
  }

  Vec(Vec&& vec)
      : m_alloc{vec.m_alloc},
        ptr{std::move(vec.ptr)},
        len{vec.len},
        cap{vec.cap} {
    vec.ptr = nullptr;
    vec.len = 0;
    vec.cap = 0;
  }

  Vec& operator=(const Vec& other) {
    if (this == &other) return *this;

    if (m_alloc->getType() == AllocType::Pool) {
      if (ptr) m_alloc->free(ptr);
    }

    m_alloc = other.m_alloc;

    ptr = static_cast<T*>(m_alloc->allocate(sizeof(T) * other.cap, alignof(T)));
    if (!ptr) panic("Vec copy assignment: allocation failed");

    len = other.len;
    cap = other.cap;

    std::copy(other.ptr, other.ptr + other.len, ptr);

    return *this;
  }

  Vec& operator=(Vec&& other) noexcept {
    if (this == &other) return *this;

    if (ptr && m_alloc && m_alloc->getType() == AllocType::Pool) {
      m_alloc->free(ptr);
    }

    m_alloc = other.m_alloc;
    ptr = other.ptr;
    len = other.len;
    cap = other.cap;

    other.ptr = nullptr;
    other.len = 0;
    other.cap = 0;

    return *this;
  }

  T& operator[](size_t i) noexcept {
    return ptr[i];
  }

  const T& operator[](size_t i) const noexcept {
    return ptr[i];
  }

  std::optional<T&> at(size_t i) noexcept {
    if (i >= len) return std::nullopt;
    return ptr[i];
  }

  std::optional<const T&> at(size_t i) const noexcept {
    if (i >= len) return std::nullopt;
    return ptr[i];
  }

  T* begin() const noexcept {
    return ptr;
  }

  T* end() const noexcept {
    return ptr + len;
  }

  std::span<T> slice(size_t start, size_t end) const noexcept {
    return std::span<T>(ptr, len).subspan(start, end);
  }

  std::span<T> toSpan() const noexcept {
    return std::span<T>(ptr, len);
  }

  const char* type() const noexcept {
    return typeid(T).name();
  }

  void print() const noexcept {
    if (len == 0) {
      std::printf("[]\n");
      return;
    }
    for (size_t i = 0; i < len; i++) {
      if (i == 0) {
        std::printf("[");
        printValue(ptr[i]);
      } else if (i < len - 1) {
        std::printf(", ");
        printValue(ptr[i]);
      } else {
        std::printf(", ");
        printValue(ptr[i]);
        std::printf("]\n");
      }

      if (len == 1) std::printf("]\n");
    }
  }

  void print(std::span<T> sli) const noexcept {
    if (len == 0) {
      std::printf("[]\n");
      return;
    }
    for (size_t i = 0; i < sli.size(); i++) {
      if (i == 0) {
        std::printf("[");
        printValue(sli[i]);
      } else if (i < sli.size() - 1) {
        std::printf(", ");
        printValue(sli[i]);
      } else {
        std::printf(", ");
        printValue(sli[i]);
        std::printf("]\n");
      }

      if (len == 1) std::printf("]\n");
    }
  }

  void printInfo() const noexcept {
    std::printf("length: ");
    printValue(len);
    std::printf(", capacity: ");
    printValue(cap);
    std::printf(", type: ");
    printValue(type());
    std::printf("\n");
  }

  T first() const noexcept {
    return ptr[0];
  }

  T last() const noexcept {
    return ptr[len - 1];
  }

  void clear() noexcept {
    len = 0;
  }

  void fill(const T& val) noexcept {
    std::fill(begin(), end(), val);
  }

  void reverse() noexcept {
    for (size_t i = 0; i < len / 2; ++i) std::swap(ptr[i], ptr[len - 1 - i]);
  }

  void sort() noexcept {
    std::sort(begin(), end());
  }

  void sortDescending() noexcept {
    std::sort(begin(), end(), [](const T& a, const T& b) { return a > b; });
  }

  template <typename Fn>
  void sortCustom(Fn fn) noexcept {
    std::sort(begin(), end(), fn);
  }

  std::optional<int> linearSearch(T x) const noexcept {
    for (size_t i = 0; i < len; i++) {
      if (ptr[i] == x) return i;
    }

    return std::nullopt;
  }

  std::optional<int> binarySearch(T x) const noexcept {
    int high = len - 1;
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

    return std::nullopt;
  }

  void extend(size_t new_size, T val = T()) noexcept {
    if (new_size > cap) m_resizeCapacity(new_size);
    for (size_t i = len; i < new_size; ++i) ptr[i] = val;
    len = new_size;
  }

  void reserve(size_t new_cap) noexcept {
    if (new_cap > cap) m_resizeCapacity(new_cap);
  }

  void shrink(size_t new_size) noexcept {
    if (len < cap && len > 0) m_resizeCapacity(new_size);
  }

  void shrinkToFit() noexcept {
    if (len < cap) m_resizeCapacity(len);
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void pushBack(S&& val) noexcept {
    if (len == cap) m_resizeCapacity(cap * 2);
    ptr[++len] = std::forward<S>(val);
  }

  template <typename... Args>
    requires(sizeof...(Args) == 1 &&
             std::is_same_v<T, std::remove_cvref_t<Args>...>)
  void emplaceBack(Args&&... args) noexcept {
    if (len == cap) m_resizeCapacity(cap * 2);

    new (&ptr[len]) T(std::forward<Args>(args)...);
    ++len;
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, T>
  void insert(S&& val, size_t ind) noexcept {
    if (ind > len) ind = len;

    if (len == cap) m_resizeCapacity(cap * 2);

    for (size_t i = len; i > ind; --i) ptr[i] = ptr[i - 1];

    ptr[ind] = std::forward<S>(val);
    ++len;
  }

  void pop() noexcept {
    if (len > 0) --len;
  }

  std::optional<T> popBack() noexcept {
    if (len <= 0) return std::nullopt;

    T val = ptr[len - 1];
    --len;

    return val;
  }

  T popBackUnsafe() noexcept {
    T val = ptr[len - 1];
    --len;

    return val;
  }

  void orderedRemove(size_t ind) noexcept {
    if (ind >= len) return;
    for (size_t i = ind; i + 1 < len; ++i) ptr[i] = ptr[i + 1];
    --len;
  }

  void unorderedRemove(size_t ind) noexcept {
    if (ind >= len) return;
    ptr[ind] = ptr[len - 1];
    --len;
  }

  void deleteVal(T val) noexcept {
    for (int i = len - 1; i >= 0; --i)
      if (ptr[i] == val) orderedRemove(i);
  }

  void deleteValUnordered(T val) noexcept {
    for (int i = len - 1; i >= 0; --i)
      if (ptr[i] == val) unorderedRemove(i);
  }

  template <typename Fn>
  void mapIter(Fn fn) noexcept {
    for (size_t i = 0; i < len; ++i) fn(ptr[i]);
  }

  template <typename Rtype>
    requires std::is_arithmetic_v<Rtype> && std::is_arithmetic_v<T>
  Rtype sum() const noexcept {
    Rtype ttl = 0;
    for (size_t i = 0; i < len; ++i) ttl += ptr[i];
    return ttl;
  }

  template <typename Rtype>
    requires std::is_arithmetic_v<Rtype> && std::is_arithmetic_v<T>
  Rtype prod() const noexcept {
    Rtype ttl = 1;
    for (size_t i = 0; i < len; ++i) ttl *= ptr[i];
    return ttl;
  }

  template <typename Rtype, typename Fn>
  Rtype foldl(size_t init, Fn fn) const noexcept {
    Rtype ttl = init;
    for (size_t i = 0; i < len; ++i) fn(ttl, ptr[i]);
    return ttl;
  }

  template <typename Rtype, typename Fn>
  std::optional<Rtype> reduce(Fn fn) const noexcept {
    if (len <= 0) return std::nullopt;
    if (len == 1) return static_cast<Rtype>(ptr[0]);

    Rtype ttl = static_cast<Rtype>(ptr[0]);
    for (size_t i = 1; i < len; ++i) fn(ttl, ptr[i]);
    return ttl;
  }

private:
  void m_resizeCapacity(size_t new_cap) {
    size_t old_bytes = cap * sizeof(T);
    size_t new_bytes = new_cap * sizeof(T);

    if (m_alloc->supportsResize()) {
      T* new_vec = static_cast<T*>(
          m_alloc->resize(ptr, old_bytes, new_bytes, alignof(T)));
      if (!new_vec) panic("Allocator resize failed");

      ptr = new_vec;
      cap = new_cap;
    } else {
      T* new_vec =
          static_cast<T*>(m_alloc->allocate(sizeof(T) * new_cap, alignof(T)));
      if (!new_vec) panic("Allocator resize failed");

      for (size_t i = 0; i < len; ++i) new (&new_vec[i]) T(std::move(ptr[i]));

      for (size_t i = 0; i < len; ++i) ptr[i].~T();

      m_alloc->free(ptr);
      ptr = new_vec;
      cap = new_cap;
    }
  }
};
