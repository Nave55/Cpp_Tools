#include <cstddef>
#include <vector>
#include <algorithm>
#include <memory_resource>

const int KB = 1024;
const int MB = KB * 1024;
const int GB = MB * 1024;

class ArenaResource : public std::pmr::memory_resource {
private:
   std::size_t               m_blockSize;
   std::vector<void*>        m_blocks;
   char*                     m_current = nullptr;
   std::size_t               m_remaining = 0;

public:
   explicit ArenaResource(std::size_t blockSize)
      : m_blockSize(blockSize) {}

   ~ArenaResource() {
      for (void* b : m_blocks) ::operator delete(b);
   }

   ArenaResource(const ArenaResource&) = delete;
   ArenaResource& operator=(const ArenaResource&) = delete;

protected:
   void* do_allocate(std::size_t bytes, std::size_t alignment) override {
      if (bytes > m_remaining) {
         std::size_t size = std::max(m_blockSize, bytes);
         void* block = ::operator new(size);
         m_blocks.push_back(block);
         m_current   = static_cast<char*>(block);
         m_remaining = size;
      }

      void* aligned = m_current;
      std::size_t space = m_remaining;
      if (!std::align(alignment, bytes, aligned, space)) 
      throw std::bad_alloc{};
      m_current   = static_cast<char*>(aligned) + bytes;
      m_remaining = space - bytes;
      return aligned;
   }

    void do_deallocate(void*, std::size_t, std::size_t) noexcept override {
        // all freed at once by destructor/reset()
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

  public:
    void reset() {
        for (auto* b : m_blocks) ::operator delete(b);
        m_blocks.clear();
        m_current = nullptr;
        m_remaining = 0;
    }
};

// int main() {
//    ArenaResource arena(MB);

//    {
//       std::pmr::vector<std::string> names(&arena);
//       names = { "Alice", "Bob", "Carol" };
//       names.push_back("Dave");
//       for (auto& s : names) std::cout << s << "\n";
//    }

//    arena.reset();
// }
