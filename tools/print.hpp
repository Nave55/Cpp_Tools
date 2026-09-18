#pragma once

#include <array>
#include <iostream>
#include "../containers/pair.hpp"

// Overload << for printing arrays
template <typename T, size_t N>
std::ostream& operator<<(std::ostream& os, const std::array<T, N>& arr) {
  os << "[";
  for (size_t i{0}; i < arr.size(); ++i) {
    os << arr[i];
    if (i < arr.size() - 1) {
      os << ",";
    }
  }
  os << "]\n";
  return os;
}

// Overload << for printing pairs
template <typename T, typename S>
std::ostream& operator<<(std::ostream& os, const Pair<T, S>& pair) {
  os << "[" << pair.x << ", " << pair.y << "]\n";
  return os;
}

template <typename T, std::size_t N>
auto printCArr(const T (&arr)[N]) -> void {
  for (size_t i{0}; i < N; ++i) {
    if (i == 0) {
      std::cout << "[" << arr[i] << ", ";
    } else if (i > 0 && i < N - 1) {
      std::cout << arr[i] << ", ";
    } else {
      std::cout << arr[i] << "]\n";
    }
  }
}

template <typename T, std::size_t N>
auto printCArr(const T (&arr)[N]) -> void {
  for (size_t i{0}; i < N; ++i) {
    if (i == 0) {
      std::cout << "[" << arr[i] << ", ";
    } else if (i > 0 && i < N - 1) {
      std::cout << arr[i] << ", ";
    } else {
      std::cout << arr[i] << "]\n";
    }
  }
}
