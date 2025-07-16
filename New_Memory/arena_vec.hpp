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
    /**
     * Constructs an empty vector with an initial capacity of 10 elements.
     *
     * Allocates memory for 10 elements from the specified arena. The vector's
     * length is initialized to 0, and its capacity is set to 10. This constructor
     * sets up the vector for use by allocating the necessary memory but does not
     * initialize any elements.
     *
     * @param arena The arena from which to allocate memory for the vector.
     */
    explicit Vec(Arena &arena) 
        : m_arena{&arena}
        , m_vec{ m_arena->alloc<T>(10) }
        , m_len{0}
        , m_capacity{10} 
        {}

    /**
     * Constructs a vector of size `sz` and capacity `cap` using memory from the
     * given arena. The elements of the vector are default-constructed.
     *
     * @param arena The arena to allocate the vector from.
     * @param sz The size and capacity of the vector.
     */
    explicit Vec(Arena &arena, size_t sz)
        : m_arena{&arena} 
        , m_vec{ m_arena->alloc<T>(sz) }
        , m_len{sz}
        , m_capacity{sz} {
            for (size_t i = 0; i < sz; ++i) {
                m_vec[i] = T();
            }
        }

    /**
     * Constructs a vector of size `sz` and capacity `cap` using memory from the
     * given arena. The elements of the vector are default-constructed.
     *
     * @param arena The arena to allocate the vector from.
     * @param sz The size of the vector.
     * @param cap The capacity of the vector.
     */
    explicit Vec(Arena &arena, size_t sz, size_t cap)
        : m_arena{&arena}    
        , m_vec{ m_arena->alloc<T>(cap) }
        , m_len{sz}
        , m_capacity{cap} {
            for (size_t i = 0; i < sz; ++i) {
                m_vec[i] = T();
            }
        }

    /**
     * Constructs a vector from an initializer list. The vector's capacity is set
     * to the size of the initializer list, and all elements are copied from the
     * list to the vector.
     *
     * @param arena The arena to allocate the vector from.
     * @param lst The initializer list to copy into the vector.
     */
    explicit Vec(Arena &arena, std::initializer_list<T> lst)
        : m_arena{&arena} 
        , m_vec{ m_arena->alloc<T>(lst.size()) }
        , m_len{lst.size()}
        , m_capacity{lst.size()} {
            std::copy(lst.begin(), lst.end(), m_vec);
        }

    /**
     * Constructs a vector from an initializer list, with a capacity of either
     * the size of the list, or the given capacity, whichever is larger.
     *
     * @param arena The arena to allocate the vector from.
     * @param lst The initializer list to copy into the vector.
     * @param cap The capacity of the vector, or 0 to use the size of the list.
     */
    explicit Vec(Arena &arena, std::initializer_list<T> lst, size_t cap)
        : m_arena{&arena} 
        , m_vec{ m_arena->alloc<T>(std::max(cap, lst.size())) }
        , m_len{lst.size()}
        , m_capacity{std::max(cap, lst.size())} {
            std::copy(lst.begin(), lst.end(), m_vec);
        }

    /**
     * Accesses the element at the given index position in the vector.
     * If the index is out of bounds, throws a std::out_of_range exception.
     * 
     * @param i The index position of the element to access.
     * @returns A non-const reference to the element at the given index.
     */
    auto operator[](size_t i) -> T& {
        // assert(i < m_len);
        if (i >= m_len) throw std::out_of_range{"m_vector::operator[]"}; 
        return m_vec[i]; 
    }

    /**
     * Accesses the element at the given index position in the vector.
     * 
     * @param i The index position of the element to access.
     * @return A const reference to the element at the specified index position.
     * @throws std::out_of_range If the index is out of bounds (i.e. greater or equal to size()).
     */
    auto operator[](size_t i) const -> const T& {
        // assert(i < m_len);
        if (i >= m_len) throw std::out_of_range{"m_vector::operator[]"}; 
        return m_vec[i]; 
    }
    
    /**
     * Returns the total number of elements that the vector can hold without
     * resizing.
     * 
     * @return The current capacity (maximum number of elements) of the vector.
     */
    auto capacity() const noexcept -> size_t { return m_capacity; }

    /**
     * Returns the number of elements currently stored in the vector.
     * 
     * @return The current size (number of elements) of the vector.
     */
    auto size() const -> size_t { return m_len; }

    /**
     * @brief Returns a pointer to the beginning of the vector.
     * @details
     * This is a pointer to the first element in the vector.
     * This is useful for range checking and iterating over the vector.
     */
    auto begin() const -> T* { return &m_vec[0]; }

    /**
     * @brief Returns a pointer to the end of the vector.
     * @details
     * This is a pointer to the element after the last element in the vector.
     * This is useful for range checking and iterating over the vector.
     */
    auto end() const -> T* { return &m_vec[m_len]; }

    /**
     * Prints the contents of the vector.
     * @details
     * Prints the vector elements separated by commas.
     * Useful for debugging purposes.
     */
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

    /**
     * Prints the contents of the vector and its metadata.
     * @details
     * Prints the vector elements separated by commas, the length and capacity of the vector.
     * Useful for debugging purposes.
     */
    auto printInfo() const -> void {
        std::cout << "vector: ";
        print();
        std::cout << "length: " << m_len << ", capacity: " << m_capacity << "\n";
    }

    /**
     * Prints the type name of the elements stored in the vector.
     */
    auto type() const -> void { std::cout << typeid(*m_vec).name(); }

    /**
     * Returns the first element of the vector.
     * @return The first element of the vector.
     * @throws std::out_of_range if the vector is empty.
     */
    auto first() const -> T { return m_vec[0]; }

    /**
     * Returns the last element of the vector.
     * @return The last element of the vector.
     * @throws std::out_of_range if the vector is empty.
     */
    auto last() const -> T { return m_vec[m_len - 1]; }

    /**
     * Clears the vector of all elements, effectively emptying it.
     *
     * The vector's capacity is not modified.
     */
    auto clear() -> void { for (size_t i = 0; i < m_len; i++) m_vec[i] = T(); }

    /**
     * Fills the vector with the specified value.
     *
     * @param val The value to fill the vector with.
     */
    auto fill(T val) -> void { std::fill(m_vec, m_vec + m_len, val); }

    /**
     * Sorts the vector in ascending order using the std::sort algorithm.
     *
     * The vector's elements are sorted using the < operator.
     */
    auto sort_vec() -> void { std::sort(begin(), end()); }

    /**
     * Searches the vector for a given element using a linear search algorithm.
     *
     * @param x The element to search for.
     * @return The index of the element if it is found, -1 otherwise.
     */
    auto linear_search(T x) const -> int {
        for (size_t i = 0; i < m_len; i++) {
            if (m_vec[i] == x) return i;
        }

        return -1;
    }

    /**
     * Searches the vector for a given element using a binary search algorithm.
     *
     * @param x The element to search for.
     * @return The index of the element if it is found, -1 otherwise.
     * @pre The vector must be sorted in ascending order.
     */
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

    /**
     * Resizes the vector to the specified size.
     *
     * If the new size is less than or equal to the current size, the vector is
     * truncated to the specified size.
     *
     * If the new size is greater than the current size but less than or equal to
     * the current capacity, the vector is expanded to the specified size with
     * the remaining elements being default constructed.
     *
     * If the new size is greater than the current capacity, the vector is
     * reallocated to double its current capacity, and the new size is set to
     * the specified size.
     *
     * @param new_size The new size of the vector.
     */
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

        // create variables for old and new alloc bytes
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

    /**
     * Reserves a minimum amount of space for the vector.
     *
     * If the new size is greater than the current capacity, the vector is
     * reallocated to double its current capacity, but no more than the
     * specified size. If the new size is less than or equal to the current
     * capacity, this function does nothing.
     *
     * @param new_size The new minimum capacity of the vector.
     */
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

    /**
     * Reduces the capacity of the vector to match its current size.
     *
     * If the current length of the vector is greater than 0 but less than
     * its current capacity, the vector's allocation is resized to fit
     * the number of elements it currently holds. This may involve reallocating
     * memory to a smaller block and updating the internal pointer and capacity.
     * The function ensures that no memory is wasted on unused capacity.
     * An assertion ensures that the resize operation is successful.
     */

    auto shrink_to_fit() -> void {
        if (m_len > 0 && m_len < m_capacity) {

            // create variables for old and new alloc bytes
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

    /**
     * Add a new element to the end of the vector.
     *
     * This method will double the capacity of the vector if it is full.
     * The input type S must be convertible to T, otherwise an
     * assertion is triggered and the function does nothing.
     *
     * @param val The value to add to the vector.
     */
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
    
    /**
     * Constructs a new object in-place at the end of the vector by passing the
     * given arguments to its constructor.
     *
     * @param args The arguments to pass to the constructor.
     */
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

    /**
     * Inserts the given value at the specified index in the vector.
     *
     * If the current capacity of the vector is equal to its length, the vector's
     * capacity is doubled to accommodate the new element. The elements from the
     * specified index onwards are shifted to the right to make room for the new
     * element.
     *
     * @param val The value to insert into the vector.
     * @param ind The index at which to insert the value.
     * @throws std::out_of_range if the index is out of bounds.
     */
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

    /**
     * Removes the last element from the vector, effectively reducing the 
     * container size by one. The last element is reset to its default 
     * value. This operation does not free memory, but simply adjusts the 
     * length of the vector.
     * 
     * @pre The vector must not be empty, i.e., m_len > 0.
     */
    auto pop() -> void {
        assert(m_len > 0);

        // set the last element to 0
        m_vec[m_len - 1] = T();

        // decrement the length
        --m_len; 
    }

    /**
     * Removes the last element from the vector and returns it. The last element is 
     * reset to its default value. This operation does not free memory, but simply 
     * adjusts the length of the vector.
     * 
     * @pre The vector must not be empty, i.e., m_len > 0.
     * @return The last element of the vector.
     */
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

    /**
     * Removes the element at index `ind` from the vector, shifting all elements 
     * to the right of it one position to the left. The last element in the vector is 
     * reset to its default value. This operation does not free memory, but simply 
     * adjusts the length of the vector.
     * 
     * @pre The vector must not be empty, i.e., m_len > 0.
     * @param ind The index of the element to remove.
     */
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

    /**
     * Removes the element at index `ind` from the vector by swapping it with the
     * last element and then calling pop_back(). This operation does not free
     * memory, but simply adjusts the length of the vector. This is an O(1)
     * operation.
     * 
     * @pre The vector must not be empty, i.e., m_len > 0.
     * @param ind The index of the element to remove.
     */
    auto unordered_remove(size_t ind) -> void {
        if (ind >= m_len || m_len == 0) return;

        if (m_len == 1) pop();
        else {
            // swap the element to be removed with the last element and pop_back
            m_vec[ind] = pop_back(); 
        }
    }
};
