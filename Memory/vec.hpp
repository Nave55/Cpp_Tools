#pragma once

#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <cstring>
// #include "arena_alloc.hpp"

template <typename T>
class Vec {
private:
    T *m_vec {nullptr};
    size_t m_len {0};
    size_t m_capacity {0};

public:
    explicit Vec() 
        : m_vec{new T[10]}, m_len{0}, m_capacity{10} {
    }

    explicit Vec(size_t sz) 
        : m_vec{new T[sz](T())}, m_len{sz}, m_capacity{sz} {
    }

    explicit Vec(size_t sz, size_t cap) 
        : m_vec{new T[std::max(sz, cap)](T())}, m_len{sz}, m_capacity{std::max(cap, sz)} {
            // memset(m_vec, T(), sizeof(T) * sz);
    }

    explicit Vec(std::initializer_list<T> lst) 
        : m_vec{new T[lst.size()]}, m_len{lst.size()}, m_capacity{lst.size()} {
            std::copy(lst.begin(), lst.end(), m_vec);
    }

    explicit Vec(std::initializer_list<T> lst, size_t cap) 
        : m_vec{new T[std::max(cap, lst.size())]}, m_len{lst.size()}, m_capacity{std::max(cap, lst.size())} {
            std::copy(lst.begin(), lst.end(), m_vec);
    }

    Vec(Vec&& other) noexcept 
    : m_vec{other.m_vec}, m_len{other.m_len} {
        other.m_vec = nullptr;
        other.m_len = 0;
        other.m_capacity = 0;
    }

    auto operator=(Vec&& other) noexcept -> Vec& {
        if (this != &other) {
            delete[] m_vec;
            m_vec = other.m_vec;
            m_len = other.m_len;
            m_capacity = other.m_capacity;
            other.m_vec = nullptr;
            other.m_len = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    ~Vec() { if (m_vec != nullptr)  delete[] m_vec; }

    auto operator[](size_t i) -> T& {
        // assert(i < m_len);
        if (i >= m_len) throw std::out_of_range{"m_vector::operator[]"}; 
        return m_vec[i]; 
    }

    auto operator[](size_t i) const -> const T& {
        // assert(i < m_len);
        if (i >= m_len) throw std::out_of_range{"m_vector::operator[]"}; 
        return m_vec[i]; 
    }

    auto capacity() const noexcept -> size_t { return m_capacity; }

    auto size() const -> size_t { return m_len; }

    auto begin() const -> T* { return &m_vec[0]; }

    auto end() const -> T* { return &m_vec[m_len]; }

    auto print() const -> void {
        if (m_len == 0) {
            std::cout << "[]\n";
            return;
        }
        for (size_t i = 0; i < m_len; i++) {
            if (i == 0) std::cout << "[" << m_vec[i];
            else if (i < m_len - 1) std::cout << ", " << m_vec[i];
            else std::cout << ", " << m_vec[i] << "]\n";

            if (m_len == 1) std::cout << "]\n";
        }
    }

    auto printInfo() const -> void {
        std::cout << "vector: ";
        print();
        std::cout << "length: " << m_len << ", capacity: " << m_capacity << "\n";
    }

    auto type() const -> void { std::cout << typeid(*m_vec).name(); }

    auto first() const -> T { return m_vec[0]; }

    auto last() const -> T { return m_vec[m_len - 1]; }

    auto clear() -> void { for (size_t i = 0; i < m_len; i++) m_vec[i] = T(); }

    auto fill(T val) -> void { std::fill(m_vec, m_vec + m_len, val); }

    auto sort_vec() -> void { std::sort(begin(), end()); }

    auto linear_search(T x) const -> int {
        for (size_t i = 0; i < m_len; i++) {
            if (m_vec[i] == x) return i;
        }
        return -1;
    }

    auto binary_search(T x) const -> int {
        int high = m_len - 1;
        int low = 0;

        while (low <= high) {
            int mid = low + ((high - low) / 2);
            if (m_vec[mid] == x) return mid;
            else if (m_vec[mid] > x) high = mid - 1;
            else low = mid + 1;
        }

        return -1;
    }

    auto resize(size_t sz) -> void {
        if (sz == m_len) return;
        if (sz < m_len) {
            m_len = sz;
            return;
        }

        // Allocate new memory
        T* new_m_vec = new T[sz](T());

        // Move elements from old m_vec and zero the rest
        // memcpy(new_m_vec, m_vec, sz * sizeof(T));
        for (size_t i = 0; i < sz; i++) {
            if (i < m_len) new_m_vec[i] = std::move(m_vec[i]);
            else new_m_vec[i] = T();
        }

        // Free the old memory
        delete[] m_vec;

        // Update the m_vector pointer and size
        m_vec = new_m_vec;
        m_len = sz;
        m_capacity = sz;
    }

    auto reserve(size_t capacity) -> void {
        if (capacity > m_capacity) {
            T* new_m_vec = new T[capacity];
            
            // cpy existing elements
            // memcpy(new_m_vec, m_vec, m_len * sizeof(T));
            for (size_t i = 0; i < m_len; ++i) 
                new_m_vec[i] = std::move(m_vec[i]);
            
            delete[] m_vec;
            m_vec = new_m_vec;
            m_capacity = capacity;
        }
    }

    auto shrink_to_fit() -> void {
        if (m_len > 0 && m_len < m_capacity) {
            T* new_m_vec = new T[m_len];

            // Copy existing elements
            // memcpy(new_m_vec, m_vec, m_len * sizeof(T));
            for (size_t i = 0; i < m_len; ++i) {
                new_m_vec[i] = std::move(m_vec[i]);
            }

            delete[] m_vec;
            m_vec = new_m_vec;
            m_capacity = m_len;
        }
    }

    template <typename S>
    auto push_back(S val) -> void {
        if constexpr (!std::is_same<decltype(val), T>::value) {
            std::cout << "push_back failed: Type mismatch. Expected " << typeid(T).name() << ", got " << typeid(val).name() << "\n";
            return;
        }
        
        // Check if the current capacity is equal to the length of the vector
        if (m_capacity == m_len) {
            // Allocate new memory with double the capacity
            T* new_m_vec = new T[m_capacity * 2 ];

            // Copy existing elements
            // memcpy(new_m_vec, m_vec, m_len * sizeof(T));
            for (size_t i = 0; i < m_len; ++i)
                new_m_vec[i] = std::move(m_vec[i]);

            // Free the old memory
            delete[] m_vec;

            // Update the m_vector pointer and capacity
            m_vec = new_m_vec;
            m_capacity *= 2;
        }

        // Check if m_len is within the bounds of the array
        if (m_len >= m_capacity) {
            throw std::out_of_range("Array subscript out of bounds");
        }

        // Add the new element
        m_vec[m_len] = val;

        // Increase the size
        ++m_len;
    }

    template <typename... Args>
    auto emplace_back(Args&&... args) -> void {
        // Ensure that T is constructible from the given arguments
        if constexpr (!std::is_constructible<T, Args&&...>::value) {
            std::cout << "emplace_back failed: Type mismatch. Cannot construct type " << typeid(T).name()
                      << " with provided arguments.\n";
            return;
        }

        // Prevent unintended implicit conversions by checking exact matches
        if constexpr (!(std::is_same_v<T, std::decay_t<Args>> && ...)) {
            std::cout << "emplace_back failed: Argument types are not an exact match for " << typeid(T).name() << "\n";
            return;
        }

        // Check if the current capacity is equal to the length of the vector
        if (m_capacity == m_len) {
            // Allocate new memory with double the capacity
            T* new_m_vec = new T[m_capacity * 2];

            // Copy existing elements
            // memcpy(new_m_vec, m_vec, m_len * sizeof(T));
            for (size_t i = 0; i < m_len; ++i)
                new_m_vec[i] = std::move(m_vec[i]);

            // Free the old memory
            delete[] m_vec;

            // Update the m_vector pointer and capacity
            m_vec = new_m_vec;
            m_capacity *= 2;
        }

        // Emplace the new element
        try {
            new (m_vec + m_len) T(std::forward<Args>(args)...);
        } catch (...) {
            // Deallocate the memory allocated by resize
            delete[] m_vec;
            m_vec = nullptr;
            m_len = 0;
            m_capacity = 0;
            throw; // Re-throw the exception
        }

        // Increase the size
        ++m_len;
    }

    auto insert(T val, size_t ind) -> void {
        if (m_capacity == 0) return;
        if (m_capacity == m_len) {
            // Allocate new memory
            T* new_m_vec = new T[m_len * 2];
        
            // Copy existing elements
            for (size_t i = 0; i < m_len; ++i)
                if (i < ind) new_m_vec[i] = std::move(m_vec[i]);
                else {
                    if (i == ind) new_m_vec[i] = val;
                    new_m_vec[i + 1] = std::move(m_vec[i]);
                };
            
            // Free the old memory
            delete[] m_vec;

            // Update the m_vector pointer and size
            m_vec = new_m_vec;
            m_capacity = m_len * 2;
        } 
        else {
            // Shift elements to the right to make room for the new element
            for (int i = m_len; i > -1; --i) {
                if (static_cast<size_t>(i) > ind) 
                    std::swap(m_vec[i], m_vec[i - 1]);
                else if (static_cast<size_t>(i) == ind) 
                    m_vec[i] = val;
            }
        }

        ++m_len;
    }

    auto pop() -> void {
        assert(m_len > 0);

        // set the last element to 0
        m_vec[m_len - 1] = T();

        // decrement the length
        --m_len; 
    }

    auto pop_back() -> T {
        assert(m_len > 0);

        // Get the last element
        T val = m_vec[m_len - 1];

        // set the last element to 0
        m_vec[m_len - 1] = T();

        // decrement the length
        --m_len; 

        return val;
    }


    auto ordered_remove(size_t ind) -> void {
        if (ind >= m_len || m_len == 0) return;

        if (m_len == 1) pop_back();
        else {
            // shift elements to the left
            for (size_t i = ind; i < m_len - 1; ++i) 
                std::swap(m_vec[i], m_vec[i + 1]);
        }
        
        // decrement the length
        --m_len;
    }

    auto unordered_remove(size_t ind) -> void {
        if (ind >= m_len || m_len == 0) return;

        if (m_len == 1) pop();
        else {
            // swap the element to be removed with the last element and pop_back
            m_vec[ind] = pop_back(); 
        }
    }
};
