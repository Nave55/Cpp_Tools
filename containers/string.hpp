#pragma once

#include <algorithm>
#include <cstring>
#include <optional>
#include <string_view>
#include "../memory/allocators.hpp"
#include "pair.hpp"

class String {
private:
  MemAllocator* m_alloc;

public:
  size_t len = 0;
  size_t cap = 0;  // total bytes including space for '\0'
  char* ptr{nullptr};

public:
  // empty string with initial capacity (including room for '\0')
  explicit String(size_t cap = 16, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{0},
        cap{cap},
        ptr{static_cast<char*>(
            m_alloc->allocate(sizeof(char) * cap, alignof(char)))} {
    if (sizeof(char) * cap > m_alloc->getChunkSize()) {
      panic("Initial String capacity does not fit in a pool chunk");
    }
    ptr[0] = '\0';
  }

  // from C-string
  explicit String(const char* str, MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{std::strlen(str)},
        cap{len + 1},
        ptr{static_cast<char*>(
            m_alloc->allocate(sizeof(char) * cap, alignof(char)))} {
    if (sizeof(char) * cap > m_alloc->getChunkSize())
      panic("Initial String capacity does not fit in a pool chunk");

    std::memcpy(ptr, str, len + 1);  // includes '\0'
  }

  // from C-string
  explicit String(const char* str, size_t cap,
                  MemAllocator& alloc = arena_alloc)
      : m_alloc{&alloc},
        len{std::strlen(str)} {
    if (cap < len + 1)
      panic("String(cap): capacity too small for input string");

    this->cap = cap;

    ptr = static_cast<char*>(
        m_alloc->allocate(sizeof(char) * cap, alignof(char)));

    if (!ptr) panic("String(cap): allocation failed");

    if (sizeof(char) * cap > m_alloc->getChunkSize())
      panic("Initial String capacity does not fit in a pool chunk");

    std::memcpy(ptr, str, len + 1);  // includes '\0'
  }
  ~String() {
    if (ptr && m_alloc->getType() == AllocType::Pool) {
      m_alloc->free(ptr);
    }
  }

  String(const String& str)
      : m_alloc{str.m_alloc},
        len{str.len},
        cap{str.cap},
        ptr{static_cast<char*>(
            m_alloc->allocate(sizeof(char) * str.cap, alignof(char)))} {
    std::memcpy(ptr, str.ptr, len + 1);  // copy including '\0'
  }

  String(String&& str)
      : m_alloc{str.m_alloc},
        len{str.len},
        cap{str.cap},
        ptr{str.ptr} {
    str.ptr = nullptr;
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
    return ptr[i];
  }

  const char& operator[](size_t i) const noexcept {
    return ptr[i];
  }

  std::optional<char&> at(size_t i) noexcept {
    if (i >= len) return std::nullopt;
    return ptr[i];
  }

  std::optional<const char&> at(size_t i) const noexcept {
    if (i >= len) return std::nullopt;
    return ptr[i];
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

    std::memcpy(ptr + len, str, n + 1);
    len += n;

    return *this;
  }

  char* begin() const noexcept {
    return ptr;
  }

  char* end() const noexcept {
    return ptr + len;
  }

  void print() const noexcept {
    std::printf("%s\n", ptr);
  }

  void printInfo() const noexcept {
    std::printf("length: %zu, capacity: %zu\n", len, cap);
  }

  bool empty() const noexcept {
    return len == 0;
  }

  char first() const noexcept {
    return ptr[0];
  }

  char last() const noexcept {
    return ptr[len - 1];
  }

  std::optional<char> firstSafe() const noexcept {
    if (len == 0) return std::nullopt;
    return ptr[len];
  }

  std::optional<char> lastSafe() const noexcept {
    if (len == 0) return std::nullopt;
    return ptr[len - 1];
  }

  std::string_view toStringView() const noexcept {
    return std::string_view(ptr);
  }

  std::string_view slice(size_t start, size_t end) const noexcept {
    return std::string_view(ptr).substr(start, (end - start) + 1);
  }

  void clear() noexcept {
    len = 0;
    if (ptr) ptr[0] = '\0';
  }

  void rtrim() {
    while (len > 0 && isspace((unsigned char)ptr[len - 1])) {
      ptr[len - 1] = '\0';
      len--;
    }
  }

  void ltrim() {
    size_t i = 0;

    while (i < len && isspace(static_cast<unsigned char>(ptr[i]))) i++;

    if (i == 0) return;
    if (i == len) {
      ptr[0] = '\0';
      len = 0;
      return;
    }

    memmove(ptr, ptr + i, len - i);

    len -= i;
    ptr[len] = '\0';
  }

  void trim() {
    ltrim();
    rtrim();
  }

  void toLower() noexcept {
    for (size_t i = 0; i < len; ++i) {
      char tmp = ptr[i];
      if (tmp >= 65 && tmp <= 90) {
        ptr[i] = tmp + 32;
      }
    }
  }

  void toUpper() noexcept {
    for (size_t i = 0; i < len; ++i) {
      char tmp = ptr[i];
      if (tmp >= 97 && tmp <= 122) {
        ptr[i] = tmp - 32;
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
    for (size_t i = 0; i < len / 2; i++) std::swap(ptr[i], ptr[len - 1 - i]);
  }

  bool containsChar(char x) const noexcept {
    for (size_t i = 0; i < len; i++) {
      if (ptr[i] == x) return true;
    }
    return false;
  }

  std::optional<size_t> findChar(char x) const noexcept {
    for (size_t i = 0; i < len; i++)
      if (ptr[i] == x) return i;
    return std::nullopt;
  }

  bool containsSubStr(std::string_view sub) const noexcept {
    if (sub.size() > len) return false;

    for (size_t i = 0; i <= len - sub.size(); ++i) {
      size_t j = 0;
      while (j < sub.size() && ptr[i + j] == sub[j]) ++j;
      if (j == sub.size()) return true;
    }

    return false;
  }

  template <typename P = Pair<size_t, size_t>>
  std::optional<P> findSubStr(std::string_view sub) const noexcept {
    if (sub.size() > len) return std::nullopt;

    for (size_t i = 0; i <= len - sub.size(); ++i) {
      size_t j = 0;
      while (j < sub.size() && ptr[i + j] == sub[j]) ++j;
      if (j == sub.size()) return std::optional<P>({i, i + j - 1});
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
    ptr[len++] = std::forward<S>(val);
    ptr[len] = '\0';
  }

  template <typename S>
    requires std::is_same_v<std::remove_cvref_t<S>, char>
  void insert(S&& val, size_t ind) noexcept {
    if (ind > len) ind = len;
    if (len + 1 >= cap) m_resizeCapacity(cap * 2);
    memmove(ptr + ind + 1, ptr + ind, len - ind + 1);
    ptr[ind] = std::forward<S>(val);
    ++len;
  }

  void pop() noexcept {
    if (len > 0) {
      --len;
      ptr[len] = '\0';
    }
  }

  std::optional<char> popBack() noexcept {
    if (len == 0) return std::nullopt;
    char val = ptr[len - 1];
    --len;
    ptr[len] = '\0';
    return val;
  }

  char popBackUnsafe() noexcept {
    char val = ptr[len - 1];
    --len;
    ptr[len] = '\0';
    return val;
  }

  void orderedRemove(size_t ind) noexcept {
    if (ind >= len) return;
    memmove(ptr + ind, ptr + ind + 1, (len - ind - 1));
    --len;
    ptr[len] = '\0';
  }

  void unorderedRemove(size_t ind) noexcept {
    if (ind >= len) return;
    ptr[ind] = ptr[len - 1];
    --len;
    ptr[len] = '\0';
  }

  void deleteVal(char val) noexcept {
    for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
      if (ptr[i] == val) orderedRemove(static_cast<size_t>(i));
    }
  }

  void deleteValUnordered(char val) noexcept {
    for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
      if (ptr[i] == val) unorderedRemove(static_cast<size_t>(i));
    }
  }

  void deleteSubStr(std::string_view str) noexcept {
    auto sub = findSubStr(str);
    if (sub.has_value()) {
      auto [l, r] = sub.value();
      memmove(ptr + l, ptr + r + 1, len - r);
      len -= r - l + 1;
      ptr[len] = '\0';
    }
  }

private:
  void m_resizeCapacity(size_t new_cap) {
    size_t old_bytes = cap * sizeof(char);
    size_t new_bytes = new_cap * sizeof(char);

    if (m_alloc->supportsResize()) {
      char* new_str = static_cast<char*>(
          m_alloc->resize(ptr, old_bytes, new_bytes, alignof(char)));
      if (!new_str) panic("Allocator resize failed");

      ptr = new_str;
      cap = new_cap;
    } else {
      char* new_str = static_cast<char*>(
          m_alloc->allocate(sizeof(char) * new_cap, alignof(char)));
      if (!new_str) panic("Allocator resize failed");

      std::memcpy(new_str, ptr, len + 1);  // include '\0'

      m_alloc->free(ptr);
      ptr = new_str;
      cap = new_cap;
    }
  }
};
