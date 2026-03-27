#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "allocators.hpp"

class String {
private:
  MemAllocator* m_alloc;
  size_t m_len = 0;
  size_t m_capacity = 0;  // total bytes including space for '\0'
  char* m_string{nullptr};

public:
  // empty string with initial capacity (including room for '\0')
  explicit String(MemAllocator& alloc, size_t cap = 16)
      : m_alloc{&alloc},
        m_len{0},
        m_capacity{cap},
        m_string{static_cast<char*>(
            m_alloc->allocate(sizeof(char) * cap, alignof(char)))} {
    if (sizeof(char) * cap > m_alloc->getChunkSize()) {
      panic("Initial String capacity does not fit in a pool chunk");
    }
    m_string[0] = '\0';
  }

  // from C-string
  explicit String(MemAllocator& alloc, const char* str)
      : m_alloc{&alloc},
        m_len{std::strlen(str)},
        m_capacity{m_len + 1},
        m_string{static_cast<char*>(
            m_alloc->allocate(sizeof(char) * m_capacity, alignof(char)))} {
    if (sizeof(char) * m_capacity > m_alloc->getChunkSize())
      panic("Initial String capacity does not fit in a pool chunk");

    std::memcpy(m_string, str, m_len + 1);  // includes '\0'
  }

  // from C-string
  explicit String(MemAllocator& alloc, const char* str, size_t cap)
      : m_alloc{&alloc},
        m_len{std::strlen(str)} {
    if (cap < m_len + 1)
      panic("String(cap): capacity too small for input string");

    m_capacity = cap;

    m_string = static_cast<char*>(
        m_alloc->allocate(sizeof(char) * m_capacity, alignof(char)));

    if (!m_string) panic("String(cap): allocation failed");

    if (sizeof(char) * m_capacity > m_alloc->getChunkSize())
      panic("Initial String capacity does not fit in a pool chunk");

    std::memcpy(m_string, str, m_len + 1);  // includes '\0'
  }
  ~String() {
    if (m_string && m_alloc->getType() == AllocType::Pool) {
      m_alloc->free(m_string);
    }
  }

  String(const String& str)
      : m_alloc{str.m_alloc},
        m_len{str.m_len},
        m_capacity{str.m_capacity},
        m_string{static_cast<char*>(
            m_alloc->allocate(sizeof(char) * str.m_capacity, alignof(char)))} {
    std::memcpy(m_string, str.m_string, m_len + 1);  // copy including '\0'
  }

  String(String&& str)
      : m_alloc{str.m_alloc},
        m_len{str.m_len},
        m_capacity{str.m_capacity},
        m_string{str.m_string} {
    str.m_string = nullptr;
    str.m_len = 0;
    str.m_capacity = 0;
  }

  String& operator=(const String&) = delete;
  String& operator=(String&&) = delete;

  char* data() {
    return m_string;
  }

  const char* data() const {
    return m_string;
  }

  const char* c_str() const {
    return m_string;
  }

  char& operator[](size_t i) noexcept {
    if (i >= m_len) panic("String::operator[] out of bounds");
    return m_string[i];
  }

  const char& operator[](size_t i) const noexcept {
    if (i >= m_len) panic("String::operator[] out of bounds");
    return m_string[i];
  }

  String& operator+=(char c) noexcept {
    pushBack(c);
    return *this;
  }

  String& operator+=(const char* str) noexcept {
    size_t n = std::strlen(str);

    if (m_len + n + 1 > m_capacity) {
      size_t new_cap = std::max(m_capacity * 2, m_len + n + 1);
      m_resizeCapacity(new_cap);
    }

    std::memcpy(m_string + m_len, str, n + 1);
    m_len += n;

    return *this;
  }

  size_t capacity() const noexcept {
    return m_capacity;
  }
  size_t size() const noexcept {
    return m_len;
  }

  char* begin() const noexcept {
    return m_string;
  }
  char* end() const noexcept {
    return m_string + m_len;
  }

  void print() const noexcept {
    std::printf("%s\n", m_string);
  }

  void printInfo() const noexcept {
    std::printf("length: %zu, capacity: %zu\n", m_len, m_capacity);
  }

  char first() const noexcept {
    if (m_len == 0) panic("String::first on empty string");
    return m_string[0];
  }

  char last() const noexcept {
    if (m_len == 0) panic("String::last on empty string");
    return m_string[m_len - 1];
  }

  void clear() noexcept {
    m_len = 0;
    if (m_string) m_string[0] = '\0';
  }

  void toLower() noexcept {
    for (size_t i = 0; i < m_len; ++i) {
      char tmp = m_string[i];
      if (tmp >= 65 && tmp <= 90) {
        m_string[i] = tmp + 32;
      }
    }
  }

  void toUpper() noexcept {
    for (size_t i = 0; i < m_len; ++i) {
      char tmp = m_string[i];
      if (tmp >= 97 && tmp <= 122) {
        m_string[i] = tmp - 32;
      }
    }
  }

  void sort() noexcept {
    std::sort(begin(), end());
  }

  void sortDescending() noexcept {
    std::sort(begin(), end(),
              [](const char& a, const char& b) { return a > b; });
  }

  template <typename F>
  void sortCustom(F func) noexcept {
    std::sort(begin(), end(), func);
  }

  int linearSearch(char x) const noexcept {
    for (size_t i = 0; i < m_len; i++) {
      if (m_string[i] == x) return static_cast<int>(i);
    }
    return -1;
  }

  int binarySearch(char x) const noexcept {
    int high = static_cast<int>(m_len) - 1;
    int low = 0;

    while (low <= high) {
      int mid = low + ((high - low) / 2);
      if (m_string[mid] == x)
        return mid;
      else if (m_string[mid] > x)
        high = mid - 1;
      else
        low = mid + 1;
    }
    return -1;
  }

  void reserve(size_t new_cap) noexcept {
    if (new_cap > m_capacity) m_resizeCapacity(new_cap);
  }

  void shrink(size_t new_size) noexcept {
    if (new_size < m_capacity && new_size >= m_len) m_resizeCapacity(new_size);
  }

  void shrinkToFit() noexcept {
    if (m_len + 1 < m_capacity) m_resizeCapacity(m_len + 1);
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, char>
  void pushBack(S&& val) noexcept {
    if (m_len + 1 >= m_capacity) m_resizeCapacity(m_capacity * 2);
    m_string[m_len++] = std::forward<S>(val);
    m_string[m_len] = '\0';
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, char>
  void insert(S&& val, size_t ind) noexcept {
    if (ind > m_len) ind = m_len;
    if (m_len + 1 >= m_capacity) m_resizeCapacity(m_capacity * 2);

    for (size_t i = m_len + 1; i > ind; --i) m_string[i] = m_string[i - 1];

    m_string[ind] = std::forward<S>(val);
    ++m_len;
    m_string[m_len] = '\0';
  }

  void pop() noexcept {
    if (m_len == 0) panic("String must be > 0 to pop");
    --m_len;
    m_string[m_len] = '\0';
  }

  char popBack() noexcept {
    if (m_len == 0) panic("String must be > 0 to pop");
    char val = m_string[m_len - 1];
    --m_len;
    m_string[m_len] = '\0';
    return val;
  }

  void orderedRemove(size_t ind) noexcept {
    if (ind >= m_len) return;
    for (size_t i = ind; i + 1 < m_len; ++i) m_string[i] = m_string[i + 1];
    --m_len;
    m_string[m_len] = '\0';
  }

  void unorderedRemove(size_t ind) noexcept {
    if (ind >= m_len) return;
    m_string[ind] = m_string[m_len - 1];
    --m_len;
    m_string[m_len] = '\0';
  }

  void deleteVal(char val) noexcept {
    for (int i = static_cast<int>(m_len) - 1; i >= 0; --i) {
      if (m_string[i] == val) orderedRemove(static_cast<size_t>(i));
    }
  }

  void deleteValUnordered(char val) noexcept {
    for (int i = static_cast<int>(m_len) - 1; i >= 0; --i) {
      if (m_string[i] == val) unorderedRemove(static_cast<size_t>(i));
    }
  }

private:
  void m_resizeCapacity(size_t new_cap) {
    size_t old_bytes = m_capacity * sizeof(char);
    size_t new_bytes = new_cap * sizeof(char);

    if (m_alloc->supportsResize()) {
      char* new_str = static_cast<char*>(
          m_alloc->resize(m_string, old_bytes, new_bytes, alignof(char)));
      if (!new_str) panic("Allocator resize failed");

      m_string = new_str;
      m_capacity = new_cap;
    } else {
      char* new_str = static_cast<char*>(
          m_alloc->allocate(sizeof(char) * new_cap, alignof(char)));
      if (!new_str) panic("Allocator resize failed");

      std::memcpy(new_str, m_string, m_len + 1);  // include '\0'

      m_alloc->free(m_string);
      m_string = new_str;
      m_capacity = new_cap;
    }
  }
};
