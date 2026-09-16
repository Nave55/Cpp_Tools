#pragma once

#include <array>

template <size_t S, size_t E, typename T, typename Fn>
consteval std::array<T, (E - S) + 1> rangeTo(Fn fn) {
  std::array<T, (E - S) + 1> arr{};
  size_t ind = 0;
  for (size_t i = S; i <= E; ++i) arr[ind++] = fn(static_cast<T>(i));
  return arr;
}

template <size_t S, size_t E, typename T>
consteval std::array<T, (E - S) + 1> rangeTo() {
  return rangeTo<S, E, T>([](T x) { return x; });
}
