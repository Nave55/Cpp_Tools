#include <cstddef>
#include <vector>
#include <algorithm>
#include <memory_resource>
#include <string>
#include <iostream>

const int KB = 1024;
const int MB = KB * 1024;
const int GB = MB * 1024;

class Arena : public std::pmr::memory_resource {
private:
   std::size_t               m_blockSize;
   std::vector<void*>        m_blocks;
   char*                     m_current = nullptr;
   std::size_t               m_remaining = 0;

public:
   struct Marker {
         char*        current;
         std::size_t  remaining;
   };

public:
   explicit Arena(std::size_t blockSize)
      : m_blockSize(blockSize) {}

   ~Arena() {
      for (void* b : m_blocks) ::operator delete(b);
   }

   Arena(const Arena&) = delete;
   Arena& operator=(const Arena&) = delete;
   
   /// Snapshot current state (where the next allocation will come from)
    Marker get_marker() const {
        return { m_current, m_remaining };
    }

    /// Roll the arena back to a prior snapshot
    void reset_to_marker(const Marker& m) {
        m_current   = m.current;
        m_remaining = m.remaining;
    }

   void reset() {
        for (auto* b : m_blocks) ::operator delete(b);
        m_blocks.clear();
        m_current = nullptr;
        m_remaining = 0;
    }

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
};

/// On construction, we snapshot the arena; on destruction, we rewind it.
class TempArena {
    Arena&         arena_;
    Arena::Marker  mark_;

  public:
    explicit TempArena(Arena& arena)
      : arena_(arena)
      , mark_(arena.get_marker())
    {}

    ~TempArena() {
        arena_.reset_to_marker(mark_);
    }

    // make it non‑copyable/non‑movable so you can’t accidentally extend its lifetime
    TempArena(const TempArena&) = delete;
    TempArena& operator=(const TempArena&) = delete;
};

// int main() {
//    Arena arena(MB);

//    {   
//       TempArena scope(arena);
//       std::pmr::vector<std::string> names(&arena);
//       names = { "Alice", "Bob", "Carol" };
//       for (auto& s : names) std::cout << s << "\n";
//    }

//    // arena.reset();
// }
