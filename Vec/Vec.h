#pragma once
#include <cstddef>
#include <initializer_list>


template <typename T>
class Vec {
private:
    T* m_vec;
    std::size_t m_len;
    std::size_t m_capacity;

public:
    explicit Vec();
    explicit Vec(std::size_t sz);
    explicit Vec(std::size_t sz, std::size_t cap);
    explicit Vec(std::initializer_list<T> lst);
    explicit Vec(std::initializer_list<T> lst, std::size_t cap);
    Vec(Vec&& other) noexcept;
    auto operator=(Vec&& other) noexcept -> Vec&;
    ~Vec();
    auto operator[](std::size_t i) -> T&;
    auto operator[](std::size_t i) const -> const T&;
    auto capacity() const noexcept -> std::size_t;
    auto size() const -> std::size_t;
    auto begin() const -> T*;
    auto end() const -> T*;
    auto print() const -> void;
    auto printInfo() const -> void;
    auto type() const -> void;
    auto first() const -> T;
    auto last() const -> T;
    auto clear() -> void;
    auto sort_vec() -> void;
    auto linear_search(T x) const -> std::size_t;
    auto binary_search(T x) const -> std::size_t;
    auto resize(std::size_t sz) -> void;
    auto reserve(std::size_t capacity) -> void;
    auto shrink_to_fit() -> void;
    auto push_back(T val) -> void;  
    auto insert(T val, std::size_t ind) -> void;
    auto pop_back() -> T;
    auto ordered_remove(std::size_t ind) -> void;
    auto unordered_remove(std::size_t ind) -> void;

    template <typename... Args>
    auto emplace_back(Args&&... args) -> void;

    template <typename... Args>
    auto emplace(std::size_t index, Args&&... args) -> void; 
};
