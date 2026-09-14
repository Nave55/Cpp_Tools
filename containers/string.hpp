#pragma once

#include <algorithm>
#include <optional>
#include <string_view>
#include "allocators.hpp"

struct Pair {
  size_t x;
  size_t y;
};

class String {
private:
  MemAllocator* m_alloc;

public:
  size_t len = 0;
  size_t cap = 0;  // total bytes including space for '\0'
  char* string{nullptr};

public:
  // empty string with initial capacity (including room for '\0')
  explicit String(size_t cap = 16, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{0},
        cap{cap},
        string{static_cast<char*>(
            m_alloc->allocate(sizeof(char) * cap, alignof(char)))} {
    if (sizeof(char) * cap > m_alloc->getChunkSize()) {
      panic("Initial String capacity does not fit in a pool chunk");
    }
    string[0] = '\0';
  }

  // from C-string
  explicit String(const char* str, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{std::strlen(str)},
        cap{len + 1},
        string{static_cast<char*>(
            m_alloc->allocate(sizeof(char) * cap, alignof(char)))} {
    if (sizeof(char) * cap > m_alloc->getChunkSize())
      panic("Initial String capacity does not fit in a pool chunk");

    std::memcpy(string, str, len + 1);  // includes '\0'
  }

  // from C-string
  explicit String(const char* str, size_t cap,
                  MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{std::strlen(str)} {
    if (cap < len + 1)
      panic("String(cap): capacity too small for input string");

    this->cap = cap;

    string = static_cast<char*>(
        m_alloc->allocate(sizeof(char) * cap, alignof(char)));

    if (!string) panic("String(cap): allocation failed");

    if (sizeof(char) * cap > m_alloc->getChunkSize())
      panic("Initial String capacity does not fit in a pool chunk");

    std::memcpy(string, str, len + 1);  // includes '\0'
  }
  ~String() {
    if (string && m_alloc->getType() == AllocType::Pool) {
      m_alloc->free(string);
    }
  }

  String(const String& str)
      : m_alloc{str.m_alloc},
        len{str.len},
        cap{str.cap},
        string{static_cast<char*>(
            m_alloc->allocate(sizeof(char) * str.cap, alignof(char)))} {
    std::memcpy(string, str.string, len + 1);  // copy including '\0'
  }

  String(String&& str)
      : m_alloc{str.m_alloc},
        len{str.len},
        cap{str.cap},
        string{str.string} {
    str.string = nullptr;
    str.len = 0;
    str.cap = 0;
  }

  String& operator=(const String& other) {
    if (this == &other) return *this;

    if (m_alloc->getType() == AllocType::Pool && ptr) {
      m_alloc->free(ptr);
    }

    m_alloc = other.m_alloc;
    len = other.len;
    cap = other.cap;

    ptr = static_cast<char*>(
        m_alloc->allocate(sizeof(char) * cap, alignof(char)));
    if (!ptr) panic("String copy assignment: allocation failed");

    std::memcpy(ptr, other.ptr, len + 1);

    return *this;
  }

  String& operator=(String&& other) {
    if (this == &other) return *this;

    if (ptr && m_alloc && m_alloc->getType() == AllocType::Pool) {
      m_alloc->free(ptr);
    }

    m_alloc = other.m_alloc;
    len = other.len;
    cap = other.cap;
    ptr = other.ptr;

    other.ptr = nullptr;
    other.len = 0;
    other.cap = 0;

    return *this;
  };

  char& operator[](size_t i) noexcept {
    return string[i];
  }

  const char& operator[](size_t i) const noexcept {
    return string[i];
  }

  std::optional<char&> at(size_t i) noexcept {
    if (i >= len) return std::nullopt;
    return string[i];
  }

  std::optional<const char&> at(size_t i) const noexcept {
    if (i >= len) return std::nullopt;
    return string[i];
  }

  String& operator+=(char c) noexcept {
    pushBack(c);
    return *this;
  }

  String& operator+=(const char* str) noexcept {
    size_t n = std::strlen(str);

    if (len + n + 1 > cap) {
      size_t new_cap = std::max(cap * 2, len + n + 1);
      m_resizeCapacity(new_cap);
    }

    std::memcpy(string + len, str, n + 1);
    len += n;

    return *this;
  }

  char* begin() const noexcept {
    return string;
  }

  char* end() const noexcept {
    return string + len;
  }

  void print() const noexcept {
    std::printf("%s\n", string);
  }

  void printInfo() const noexcept {
    std::printf("length: %zu, capacity: %zu\n", len, cap);
  }

  bool empty() const noexcept {
    return len == 0;
  }

  char first() const noexcept {
    return string[0];
  }

  char last() const noexcept {
    return string[len - 1];
  }

  std::optional<char> firstSafe() const noexcept {
    if (len == 0) return std::nullopt;
    return string[len];
  }

  std::optional<char> lastSafe() const noexcept {
    if (len == 0) return std::nullopt;
    return string[len - 1];
  }

  std::string_view toStringView() const noexcept {
    return std::string_view(string);
  }

  std::string_view slice(size_t start, size_t end) const noexcept {
    return std::string_view(string).substr(start, (end - start) + 1);
  }

  void clear() noexcept {
    len = 0;
    if (string) string[0] = '\0';
  }

  void rtrim() {
    while (len > 0 && isspace((unsigned char)string[len - 1])) {
      string[len - 1] = '\0';
      len--;
    }
  }

  void ltrim() {
    size_t i = 0;

    while (i < len && isspace(static_cast<unsigned char>(string[i]))) i++;

    if (i == 0) return;
    if (i == len) {
      string[0] = '\0';
      len = 0;
      return;
    }

    memmove(string, string + i, len - i);

    len -= i;
    string[len] = '\0';
  }

  void trim() {
    ltrim();
    rtrim();
  }

  void toLower() noexcept {
    for (size_t i = 0; i < len; ++i) {
      char tmp = string[i];
      if (tmp >= 65 && tmp <= 90) {
        string[i] = tmp + 32;
      }
    }
  }

  void toUpper() noexcept {
    for (size_t i = 0; i < len; ++i) {
      char tmp = string[i];
      if (tmp >= 97 && tmp <= 122) {
        string[i] = tmp - 32;
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

  void reverse() noexcept {
    for (size_t i = 0; i < len / 2; i++)
      std::swap(string[i], string[len - 1 - i]);
  }

  bool containsChar(char x) const noexcept {
    for (size_t i = 0; i < len; i++) {
      if (string[i] == x) return true;
    }
    return false;
  }

  std::optional<size_t> findChar(char x) const noexcept {
    for (size_t i = 0; i < len; i++)
      if (string[i] == x) return i;
    return std::nullopt;
  }

  bool containsSubStr(const String& sub) {
    if (sub.len > len) return false;

    for (size_t i = 0; i <= len - sub.len; ++i) {
      size_t j = 0;
      while (j < sub.len && string[i + j] == sub[j]) ++j;
      if (j == sub.len) return true;
    }

    return false;
  }

  std::optional<Pair> findSubStr(const String& sub) {
    if (sub.len > len) return std::nullopt;

    for (size_t i = 0; i <= len - sub.len; ++i) {
      size_t j = 0;
      while (j < sub.len && string[i + j] == sub[j]) ++j;
      if (j == sub.len) return std::optional<Pair>({i, i + j - 1});
    }

    return std::nullopt;
  }

  void reserve(size_t new_cap) noexcept {
    if (new_cap > cap) m_resizeCapacity(new_cap);
  }

  void shrink(size_t new_size) noexcept {
    if (new_size < cap && new_size >= len) m_resizeCapacity(new_size);
  }

  void shrinkToFit() noexcept {
    if (len + 1 < cap) m_resizeCapacity(len + 1);
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, char>
  void pushBack(S&& val) noexcept {
    if (len + 1 >= cap) m_resizeCapacity(cap * 2);
    string[len++] = std::forward<S>(val);
    string[len] = '\0';
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, char>
  void insert(S&& val, size_t ind) noexcept {
    if (ind > len) ind = len;
    if (len + 1 >= cap) m_resizeCapacity(cap * 2);

    for (size_t i = len + 1; i > ind; --i) string[i] = string[i - 1];

    string[ind] = std::forward<S>(val);
    ++len;
    string[len] = '\0';
  }

  void pop() noexcept {
    if (len > 0) {
      --len;
      string[len] = '\0';
    }
  }

  std::optional<char> popBack() noexcept {
    if (len == 0) return std::nullopt;
    char val = string[len - 1];
    --len;
    string[len] = '\0';
    return val;
  }

  char popBackUnsafe() noexcept {
    char val = string[len - 1];
    --len;
    string[len] = '\0';
    return val;
  }

  void orderedRemove(size_t ind) noexcept {
    if (ind >= len) return;
    for (size_t i = ind; i + 1 < len; ++i) string[i] = string[i + 1];
    --len;
    string[len] = '\0';
  }

  void unorderedRemove(size_t ind) noexcept {
    if (ind >= len) return;
    string[ind] = string[len - 1];
    --len;
    string[len] = '\0';
  }

  void deleteVal(char val) noexcept {
    for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
      if (string[i] == val) orderedRemove(static_cast<size_t>(i));
    }
  }

  void deleteValUnordered(char val) noexcept {
    for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
      if (string[i] == val) unorderedRemove(static_cast<size_t>(i));
    }
  }

private:
  void m_resizeCapacity(size_t new_cap) {
    size_t old_bytes = cap * sizeof(char);
    size_t new_bytes = new_cap * sizeof(char);

    if (m_alloc->supportsResize()) {
      char* new_str = static_cast<char*>(
          m_alloc->resize(string, old_bytes, new_bytes, alignof(char)));
      if (!new_str) panic("Allocator resize failed");

      string = new_str;
      cap = new_cap;
    } else {
      char* new_str = static_cast<char*>(
          m_alloc->allocate(sizeof(char) * new_cap, alignof(char)));
      if (!new_str) panic("Allocator resize failed");

      std::memcpy(new_str, string, len + 1);  // include '\0'

      m_alloc->free(string);
      string = new_str;
      cap = new_cap;
    }
  }
};
