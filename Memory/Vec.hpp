#pragma once
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <assert.h>
#include <algorithm>

template <typename T>
class Vec {
private:
    T* m_vec;
    size_t m_len;
    size_t m_capacity;

public:
    explicit Vec() 
        : m_vec{new T[10]}, m_len{0}, m_capacity{10} {
            for (size_t i = 0; i < m_len; i++) m_vec[i] = 0;
    }

    explicit Vec(size_t sz) 
        : m_vec{new T[sz]}, m_len{sz}, m_capacity{sz} {
            for (size_t i = 0; i < sz; i++) m_vec[i] = 0;
    }

    explicit Vec(size_t sz, size_t cap) 
        : m_vec{new T[std::max(sz, cap)]}, m_len{sz}, m_capacity{std::max(cap, sz)} {
            for (size_t i = 0; i < sz; i++) m_vec[i] = 0;
    }

    explicit Vec(std::initializer_list<T> lst) 
        : m_vec{new T[lst.size()]}, m_len{lst.size()}, m_capacity{lst.size()} {
            std::copy(lst.begin(), lst.end(), m_vec);
    }

    explicit Vec(std::initializer_list<T> lst, size_t cap) 
        : m_vec{new T[std::max(cap, lst.size())]}, m_len{lst.size()}, m_capacity{std::max(cap, lst.size())} { 
            std::copy(lst.begin(), lst.end(), m_vec);
    }

    Vec(Vec&& other) noexcept : m_vec{other.m_vec}, m_len{other.m_len} {
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

    auto clear() -> void { for (size_t i = 0; i < m_len; i++) m_vec[i] = 0; }

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

        // Allocate new memory
        T* new_m_vec = new T[sz];

        // Move elements from old m_vec and zero the rest
        for (size_t i = 0; i < sz; i++) {
            if (i < m_len) new_m_vec[i] = std::move(m_vec[i]);
            else new_m_vec[i] = 0;
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
            for (size_t i = 0; i < m_len; ++i) {
                new_m_vec[i] = std::move(m_vec[i]);
            }
            delete[] m_vec;
            m_vec = new_m_vec;
            m_capacity = m_len;
        }
    }

    auto push_back(T val) -> void {
        // Check if the current capacity is equal to the length of the vector
        if (m_capacity == m_len) {
            // Allocate new memory with double the capacity
            T* new_m_vec = new T[m_capacity * 2 ];

            // Copy existing elements
            for (size_t i = 0; i < m_len; ++i)
                new_m_vec[i] = std::move(m_vec[i]);

            // Free the old memory
            delete[] m_vec;

            // Update the m_vector pointer and capacity
            m_vec = new_m_vec;
            m_capacity = std::max(static_cast<size_t>(10), m_capacity * 2);
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
        // Check if the current capacity is equal to the length of the vector
        if (m_capacity == m_len) {
            // Allocate new memory with double the capacity
            T* new_m_vec = new T[m_capacity * 2];

            // Copy existing elements
            for (size_t i = 0; i < m_len; ++i)
                new_m_vec[i] = std::move(m_vec[i]);

            // Free the old memory
            delete[] m_vec;

            // Update the m_vector pointer and capacity
            m_vec = new_m_vec;
            m_capacity = std::max(static_cast<size_t>(10), m_capacity * 2);
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

    template <typename... Args>
    auto emplace(size_t index, Args&&... args) -> void {
        // Check if the index is within bounds
        if (index > m_len) {
            throw std::out_of_range("Index out of range");
        }

        // Check if the current capacity is equal to the length of the vector
        if (m_capacity == m_len) {
            // Allocate new memory with double the capacity
            T* new_m_vec = new T[m_capacity * 2];

            // Copy existing elements
            for (size_t i = 0; i < m_len; ++i)
                new_m_vec[i] = std::move(m_vec[i]);

            // Free the old memory
            delete[] m_vec;

            // Update the m_vector pointer and capacity
            m_vec = new_m_vec;
            m_capacity = std::max(static_cast<size_t>(10), m_capacity * 2);
        }
    }

    auto insert(T val, size_t ind) -> void {
        if (m_capacity == 0 || ind >= m_len) {
            if (m_capacity == 0 || ind == m_len) 
                std::cout << "Please use push_back() to add elements to the vector of this size\n";
            else 
                std::cout << "Index out of range for insert\n";         
            return;   
        }

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
            m_capacity = std::max(static_cast<size_t>(10), m_capacity * 2);
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
        m_vec[m_len - 1] = 0;

        // decrement the length
        --m_len; 
    }

    auto pop_back() -> T {
        assert(m_len > 0);

        // Get the last element
        auto val = m_vec[m_len - 1];

        // set the last element to 0
        m_vec[m_len - 1] = 0;

        // decrement the length
        --m_len; 

        return val;
    }

    auto ordered_remove(size_t ind) -> void {
        if (ind >= m_len || m_len == 0) return;

        if (m_len == 1) m_vec[0] = 0;
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
