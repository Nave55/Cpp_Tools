#pragma once

#include <iostream>
#include <cassert>
#include <cstring>

constexpr const auto DEFAULT_ALIGNMENT = 2 * sizeof(void*);

/**
 * Check if a given number is a power of two.
 *
 * @param x The number to be checked.
 * @returns If the number is a power of two.
 *
 * The algorithm works by using the bitwise AND operator (&) to see if the least
 * significant bit of x is zero. If it is, then we know x is a power of two. If
 * not, then we know it is not a power of two. This is because powers of two have
 * exactly one bit set to 1, and all other bits set to 0.
 */

constexpr auto is_power_of_two(const uintptr_t &x) -> bool {
	return (x & (x - 1)) == 0;
}

/**
 * Align a pointer forward to the specified alignment. If the pointer is
 * not aligned, push the address to the next value which is aligned.
 *
 * @param ptr The pointer to be aligned.
 * @param align The alignment to be used.
 * @returns The aligned pointer.
 */
constexpr auto align_forward(const uintptr_t &ptr, const size_t &align) -> uintptr_t {
	uintptr_t p, a, modulo;

	assert(is_power_of_two(align));

	p = ptr;
	a = (uintptr_t) align;
	modulo = p & (a - 1); // Same as (p % a) but faster as 'a' is a power of two

	if (modulo != 0) {
		// If 'p' address is not aligned, push the address to the
		// next value which is aligned
		p += a - modulo;
	}
	return p;
}

struct Arena {
	unsigned char *buf;
	size_t         buf_len;
	size_t         prev_offset; // This will be useful for later on
	size_t         curr_offset;

    /**
     * Create an arena with the specified size. The arena will be zeroed initially.
     *
     * @param buf_size The size of the arena in bytes.
     * @param prev The initial value of prev_offset.
     * @param curr The initial value of curr_offset.
     */
    Arena(size_t buf_size = 1024, size_t prev = 0, size_t curr = 0)
        : buf{(unsigned char *) malloc(buf_size)}, buf_len{buf_size}, prev_offset{prev}, curr_offset{curr} {
        }

    /**
     * Allocate a block of memory from the arena with the given size and
     * alignment. If the memory block is at the end of the arena, it will
     * allocate a new block of memory at the beginning of the arena and
     * copy the old memory to the new memory. If the memory block is out of
     * bounds of the arena, this function will return NULL and assert.
     *
     * @param size The size of the memory block to be allocated.
     * @param align The alignment of the memory block to be allocated.
     * @returns A pointer to the allocated memory block, or NULL if the
     * memory block is out of bounds of the arena.
     */

    /**
     * Deallocate all the memory allocated by this arena.
     *
     * @details
     * This will deallocate the memory allocated by this arena. It will also
     * reset the `prev_offset` and `curr_offset` to 0, effectively resetting the
     * arena to its state after construction.
     */
    ~Arena() {
        free(buf);
    }

    /**
     * Allocates a block of memory from the arena with the specified size and alignment.
     * 
     * This function aligns the current offset forward to the specified alignment and checks
     * if there is enough space available in the arena to allocate the memory block. If there
     * is sufficient space, it returns a pointer to the newly allocated and zero-initialized
     * memory block. If the arena is out of memory, it returns NULL.
     *
     * @param size The size of the memory block to be allocated.
     * @param align The alignment of the memory block to be allocated. Defaults to DEFAULT_ALIGNMENT.
     * @return A pointer to the allocated memory block or NULL if the arena does not have enough space.
     */

    auto alloc(size_t size, size_t align = DEFAULT_ALIGNMENT) -> void* {
        assert(curr_offset + size <= buf_len && "Memory Allocation overflow");

        // Align 'curr_offset' forward to the specified alignment
        uintptr_t curr_ptr = (uintptr_t) buf + (uintptr_t) curr_offset;
        uintptr_t offset = align_forward(curr_ptr, align);
        offset -= (uintptr_t) buf; // Change to relative offset

        // Check to see if the backing memory has space left
        if (offset + size <= buf_len) {
            void *ptr = &buf[offset];
            prev_offset = offset;
            curr_offset = offset+size;

            // Zero new memory by default
            memset(ptr, 0, size);
            return ptr;
        }

        // Return NULL if the arena is out of memory (or handle differently)
        return NULL;
    }

    /**
     * Resize a block of memory previously allocated with `alloc` in this arena.
     * If the memory block is at the end of the arena, it will resize the memory
     * block in place. If there is not enough space at the end of the arena to
     * resize the block, it will allocate a new block of memory at the beginning
     * of the arena and copy the old memory to the new memory. If the memory
     * block is out of bounds of the arena, this function will return NULL and
     * assert.
     *
     * @param old_memory The memory block to be resized.
     * @param old_size The original size of `old_memory`.
     * @param new_size The new size of the memory block.
     * @param align The alignment of the new memory block.
     * @returns A pointer to the resized memory block, or NULL if the memory
     * block is out of bounds of the arena.
     */
    auto resize(void *old_memory, size_t old_size, size_t new_size, size_t align = DEFAULT_ALIGNMENT) -> void* {
        assert(curr_offset + new_size <= buf_len && "Memory Allocation overflow");
        unsigned char *old_mem = (unsigned char *) old_memory;

        assert(is_power_of_two(align));

        if (old_mem == NULL || old_size == 0) {
            return alloc(new_size, align);
        } else if (buf <= old_mem && old_mem < buf + buf_len) {
            if (buf + prev_offset == old_mem) {
                curr_offset = prev_offset + new_size;
                if (new_size > old_size) {
                    // Zero the new memory by default
                    memset(&buf[curr_offset], 0, new_size-old_size);
                }
                return old_memory;
            } else {
                void *new_memory = alloc(new_size, align);
                if (!new_memory) {
                    // Handle allocation failure
                    assert(0 && "Failed to allocate memory for resizing");
                    return NULL;
                }
                size_t copy_size = old_size < new_size ? old_size : new_size;
                // Copy across old memory to the new memory
                memmove(new_memory, old_memory, copy_size);
                return new_memory;
            }

        } else {
            assert(0 && "Memory is out of bounds of the buffer in this arena");
            return NULL;
        }
    }

    /**
     * Resets the arena to its original state, freeing all memory allocated through
     * the arena. 
     */
    auto free_all() -> void {
        curr_offset = 0;
        prev_offset = 0;
    }
};

struct Temp_Arena {
	Arena *arena;
	size_t prev_offset;
	size_t curr_offset;

    /**
     * \brief Temporarily sets the arena that this Temp_Arena operates on.
     *
     * This is useful for temporarily using an arena that is not the
     * default arena, and then restoring the original arena when finished.
     *
     * \param a The arena to temporarily use.
     */
    auto temp_begin(Arena *a) -> void {
        arena = a;
        prev_offset = a->prev_offset;
        curr_offset = a->curr_offset;
    }

    /**
     * \brief Restores the arena to its previous state.
     *
     * This should be called when you are finished with the temporary arena
     * and want to go back to the original arena.
     */
    auto temp_end() -> void {
        arena->prev_offset = prev_offset;
        arena->curr_offset = curr_offset;
    }
};
