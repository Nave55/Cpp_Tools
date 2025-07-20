#pragma once

#include <initializer_list>
#include "arena.hpp"

template <typename T>
class Vec {
private:
    Arena* m_arena;    
    T*     m_vec {nullptr}; 
    size_t m_len {0};
    size_t m_capacity {0};

public:
    explicit Vec(Arena &arena) 
        : m_arena{&arena}
        , m_vec{ m_arena->alloc<T>(10) }
        , m_len{0}
        , m_capacity{10} 
        {}

    explicit Vec(Arena &arena, size_t sz)
        : m_arena{&arena} 
        , m_vec{ m_arena->alloc<T>(sz) }
        , m_len{sz}
        , m_capacity{sz} {
            for (size_t i = 0; i < sz; ++i) {
                m_vec[i] = T();
            }
        }

    explicit Vec(Arena &arena, size_t sz, size_t cap)
        : m_arena{&arena}    
        , m_vec{ m_arena->alloc<T>(cap) }
        , m_len{sz}
        , m_capacity{cap} {
            for (size_t i = 0; i < sz; ++i) {
                m_vec[i] = T();
            }
        }

    explicit Vec(Arena &arena, std::initializer_list<T> lst)
        : m_arena{&arena} 
        , m_vec{ m_arena->alloc<T>(lst.size()) }
        , m_len{lst.size()}
        , m_capacity{lst.size()} {
            std::copy(lst.begin(), lst.end(), m_vec);
        }

    explicit Vec(Arena &arena, std::initializer_list<T> lst, size_t cap)
        : m_arena{&arena} 
        , m_vec{ m_arena->alloc<T>(std::max(cap, lst.size())) }
        , m_len{lst.size()}
        , m_capacity{std::max(cap, lst.size())} {
            std::copy(lst.begin(), lst.end(), m_vec);
        }

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

    auto resize(size_t new_size) -> void {
        // check if new size is less than or equal to current size
        if (new_size <= m_len) {
            m_len = new_size;
            return;
        }

        // check if new size is less than or equal to current capacity but greater than current length
        if (new_size <= m_capacity && new_size > m_len) {
            for (size_t i = m_len; i < new_size; ++i) {
                m_vec[i] = T();
            }
            m_len = new_size;
            return;
        }

        // create variables for old and new alloc size
        size_t old_alloc_size = m_capacity;
        size_t new_alloc_size = (m_capacity * 2);

        // resize allocation
        T* new_m_vec = (T*) m_arena->resize<T, T>(m_vec, old_alloc_size, new_alloc_size);
        assert(new_m_vec && "Arena resize failed!");

        // zero out new elements
        for (size_t i = m_len; i < new_size; ++i) {
            new_m_vec[i] = T();
        }

        // Update the m_vector pointer, capacity and length
        m_vec = new_m_vec;
        m_len = new_size;
        m_capacity = new_size * 2;
    }

    auto reserve(size_t new_size) -> void {
        if (new_size > m_capacity) {

            // create variables for old and new alloc bytes
            size_t old_alloc_size = m_capacity;
            size_t new_alloc_size = (m_capacity * 2);

            // resize allocation
            T* new_m_vec = (T*) m_arena->resize<T, T>(m_vec, old_alloc_size, new_alloc_size);
            assert(new_m_vec && "Arena resize failed!");

            // Update the m_vector pointer and capacity
            m_vec = new_m_vec;
            m_capacity = new_size;
        }
    }

    auto shrink_to_fit() -> void {
        if (m_len > 0 && m_len < m_capacity) {

            // create variables for old and new alloc size
            size_t old_alloc_size = m_capacity;
            size_t new_alloc_size = (m_capacity * 2);

            // resize allocation
            T* new_m_vec = (T*) m_arena->resize<T, T>(m_vec, old_alloc_size, new_alloc_size);
            assert(new_m_vec && "Arena resize failed!");

            // Update the m_vector pointer and capacity
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

            // create variables for old and new alloc bytes
            size_t old_alloc_size = m_capacity;
            size_t new_alloc_size = (m_capacity * 2);
            
            // resize allocation
            T* new_m_vec = (T*) m_arena->resize<T, T>(m_vec, old_alloc_size, new_alloc_size);
            assert(new_m_vec && "Arena resize failed!");
            
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
        // Check if we need to resize
        if (m_capacity == m_len) {

            // create variables for old and new alloc bytes
            size_t old_alloc_size = m_capacity;
            size_t new_alloc_size = (m_capacity * 2);

            // resize allocation
            T* new_m_vec = (T*) m_arena->resize<T, T>(m_vec, old_alloc_size, new_alloc_size);
            assert(new_m_vec && "Arena resize failed!");

            // Update the vector pointer and capacity
            m_vec = new_m_vec;
            m_capacity *= 2;
        }

        // Placement new to construct the object in-place
        new (&m_vec[m_len]) T(std::forward<Args>(args)...);

        // Increase the size
        ++m_len;
    }

    auto insert(T val, size_t ind) -> void {
        if (m_capacity == 0) return;
        if (m_capacity == m_len) {

            // create variables for old and new alloc bytes
            size_t old_alloc_size = m_capacity;
            size_t new_alloc_size = (m_capacity * 2);

            // resize allocation
            T* new_m_vec = (T*) m_arena->resize<T, T>(m_vec, old_alloc_size, new_alloc_size);
            assert(new_m_vec && "Arena resize failed!");

            // Update the m_vector pointer and capacity
            m_vec = new_m_vec;
            m_capacity *= 2;
        } 

        // Shift elements to the right to make room for the new element
        for (int i = m_len; i > -1; --i) {
            if (static_cast<size_t>(i) > ind) 
                std::swap(m_vec[i], m_vec[i - 1]);
            else if (static_cast<size_t>(i) == ind) 
                m_vec[i] = val;
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
