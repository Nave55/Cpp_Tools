#pragma once

#include <optional>
#include <string_view>
#include "pair.hpp"

template <size_t N>
class Str {
public:
  char ptr[N + 1];

public:
  constexpr Str(const char (&str)[N + 1]) {
    for (size_t i = 0; i < N; ++i) ptr[i] = str[i];
    ptr[N] = '\0';
  }

  ~Str() = default;
  constexpr Str(const Str& other) = default;
  constexpr Str(Str&& other) noexcept = default;
  constexpr Str& operator=(const Str& other) = default;
  constexpr Str& operator=(Str&& other) noexcept = default;

  constexpr char& operator[](size_t i) noexcept {
    return ptr[i];
  }

  constexpr const char& operator[](size_t i) const noexcept {
    return ptr[i];
  }

  std::optional<char&> at(size_t i) noexcept {
    if (i >= N) return std::nullopt;
    return ptr[i];
  }

  std::optional<const char&> at(size_t i) const noexcept {
    if (i >= N) return std::nullopt;
    return ptr[i];
  }

  constexpr char* begin() noexcept {
    return ptr;
  }

  constexpr char* end() noexcept {
    return ptr + N;
  }

  constexpr const char* begin() const noexcept {
    return ptr;
  }

  constexpr const char* end() const noexcept {
    return ptr + N;
  }

  void print() const noexcept {
    std::printf("%s\n", ptr);
  }

  constexpr char first() const noexcept {
    return ptr[0];
  }

  constexpr char last() const noexcept {
    return ptr[N - 1];
  }

  std::optional<char> firstSafe() const noexcept {
    if (N == 0) return std::nullopt;
    return ptr[0];
  }

  std::optional<char> lastSafe() const noexcept {
    if (N == 0) return std::nullopt;
    return ptr[N - 1];
  }

  std::string_view toStringView() const noexcept {
    return std::string_view(ptr, N);
  }

  std::string_view slice(size_t start, size_t end = N - 1) const noexcept {
    return std::string_view(ptr + start, (end - start + 1));
  }

  bool containsChar(char x) const noexcept {
    for (size_t i = 0; i < N; i++)
      if (ptr[i] == x) return true;
    return false;
  }

  std::optional<size_t> findChar(char x) const noexcept {
    for (size_t i = 0; i < N; i++)
      if (ptr[i] == x) return i;
    return std::nullopt;
  }

  bool containsSubStr(std::string_view sub) const noexcept {
    if (sub.size() > N) return false;

    for (size_t i = 0; i <= N - sub.size(); ++i) {
      size_t j = 0;
      while (j < sub.size() && ptr[i + j] == sub[j]) ++j;
      if (j == sub.size()) return true;
    }
    return false;
  }

  template <typename P = Pair<size_t, size_t>>
  std::optional<P> findSubStr(std::string_view sub) const noexcept {
    if (sub.size() > N) return std::nullopt;

    for (size_t i = 0; i <= N - sub.size(); ++i) {
      size_t j = 0;
      while (j < sub.size() && ptr[i + j] == sub[j]) ++j;
      if (j == sub.size()) return P{i, i + j - 1};
    }
    return std::nullopt;
  }
};
