#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include "allocators.hpp"
#include "vec.hpp"

// -----------------------------------------------------------------------------
// Node
// -----------------------------------------------------------------------------
template <typename K, typename V>
struct HashNode {
  size_t hash;
  K key;
  V value;
  HashNode* next;

  HashNode(size_t h, const K& k, const V& v)
      : hash(h),
        key(k),
        value(v),
        next(nullptr) {}

  HashNode(size_t h, K&& k, V&& v)
      : hash(h),
        key(std::move(k)),
        value(std::move(v)),
        next(nullptr) {}
};

// -----------------------------------------------------------------------------
// UnorderedMap (slab-only, Arena/Stack/Pool)
// -----------------------------------------------------------------------------
template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEq = std::equal_to<K>>
class UnorderedMap {
  using Node = HashNode<K, V>;

public:
  class iterator;

private:
  MemAllocator* m_alloc;
  size_t m_len;
  size_t m_cap;
  Node** m_buckets;
  Hash m_hash;
  KeyEq m_eq;
  bool m_can_free;  // only Pool frees

  Vec<void*> m_slabs;   // raw slab pointers
  Node* m_free;         // free list head
  size_t m_slab_size;   // nodes per slab
  size_t m_slab_bytes;  // bytes per slab

public:
  explicit UnorderedMap(MemAllocator& alloc, size_t cap = 16)
      : m_alloc(&alloc),
        m_len(0),
        m_cap(m_nextPow2(cap)),
        m_buckets(static_cast<Node**>(
            alloc.allocate(sizeof(Node*) * m_cap, alignof(Node*)))),
        m_hash(Hash{}),
        m_eq(KeyEq{}),
        m_can_free(alloc.getType() == AllocType::Pool),
        m_slabs(alloc, 0, 4),
        m_free(nullptr),
        m_slab_size(0),
        m_slab_bytes(0) {
    if (!m_buckets) panic("UnorderedMap: bucket allocation failed");
    for (size_t i = 0; i < m_cap; ++i) m_buckets[i] = nullptr;

    const size_t align = alignof(Node);
    const size_t node_size = sizeof(Node);
    const size_t chunk = m_alloc->getChunkSize();
    static_assert(node_size % align == 0,
                  "UnorderedMap: Node size must be multiple of alignment");

    switch (m_alloc->getType()) {
      case AllocType::Pool: {
        // Pool: slab = one chunk (rounded down to multiple of align)
        size_t usable = (chunk / align) * align;
        if (usable < node_size) {
          panic("UnorderedMap: usable pool chunk too small for Node");
        }
        m_slab_bytes = usable;
        m_slab_size = usable / node_size;
      } break;

      case AllocType::Arena:
      case AllocType::Stack: {
        // Arena/Stack: small fixed slabs (e.g. 64 nodes), capped by chunk
        const size_t target_nodes = 64;
        size_t bytes = node_size * target_nodes;
        if (bytes > chunk) bytes = node_size;

        if (bytes < node_size) {
          panic("UnorderedMap: arena/stack slab too small for Node");
        }

        m_slab_bytes = bytes;
        m_slab_size = bytes / node_size;
      } break;
    }

    if (m_slab_size == 0) {
      panic("UnorderedMap: slab_size computed as 0");
    }
  }

  ~UnorderedMap() {
    clear();

    if (m_can_free) {
      m_alloc->free(m_buckets);
      for (size_t i = 0; i < m_slabs.size(); ++i) {
        void* raw = m_slabs[i];
        if (raw) m_alloc->free(raw);
      }
    }
  }

  UnorderedMap(const UnorderedMap&) = delete;
  UnorderedMap& operator=(const UnorderedMap&) = delete;

  UnorderedMap(UnorderedMap&& o) noexcept
      : m_alloc(o.m_alloc),
        m_len(o.m_len),
        m_cap(o.m_cap),
        m_buckets(o.m_buckets),
        m_hash(std::move(o.m_hash)),
        m_eq(std::move(o.m_eq)),
        m_can_free(o.m_can_free),
        m_slabs(std::move(o.m_slabs)),
        m_free(o.m_free),
        m_slab_size(o.m_slab_size),
        m_slab_bytes(o.m_slab_bytes) {
    o.m_alloc = nullptr;
    o.m_buckets = nullptr;
    o.m_len = 0;
    o.m_cap = 0;
    o.m_can_free = false;
    o.m_free = nullptr;
    o.m_slab_size = 0;
    o.m_slab_bytes = 0;
  }

  UnorderedMap& operator=(UnorderedMap&& o) noexcept {
    if (this == &o) return *this;
    this->~UnorderedMap();
    new (this) UnorderedMap(std::move(o));
    return *this;
  }

  // ---------------------------------------------------------------------------
  // Basic API
  // ---------------------------------------------------------------------------
  size_t len() const {
    return m_len;
  }
  size_t cap() const {
    return m_cap;
  }

  V& operator[](const K& key) {
    if (m_len * 4 > m_cap * 3) rehash(m_cap * 2);

    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (m_cap - 1);

    Node* n = m_buckets[i];
    while (n) {
      if (n->hash == h && m_eq(n->key, key)) return n->value;
      n = n->next;
    }

    Node* newNode = m_allocNode(h, key, V{});
    newNode->next = m_buckets[i];
    m_buckets[i] = newNode;
    ++m_len;
    return newNode->value;
  }

  void insert(const K& key, const V& value) {
    if (m_len * 4 > m_cap * 3) rehash(m_cap * 2);

    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (m_cap - 1);

    Node* n = m_buckets[i];
    while (n) {
      if (n->hash == h && m_eq(n->key, key)) {
        n->value = value;
        return;
      }
      n = n->next;
    }

    Node* newNode = m_allocNode(h, key, value);
    newNode->next = m_buckets[i];
    m_buckets[i] = newNode;
    ++m_len;
  }

  bool erase(const K& key) {
    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (m_cap - 1);

    Node* prev = nullptr;
    Node* curr = m_buckets[i];

    while (curr) {
      if (curr->hash == h && m_eq(curr->key, key)) {
        if (prev)
          prev->next = curr->next;
        else
          m_buckets[i] = curr->next;

        curr->~Node();
        m_freeNode(curr);
        --m_len;
        return true;
      }
      prev = curr;
      curr = curr->next;
    }
    return false;
  }

  V* find(const K& key) {
    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (m_cap - 1);

    Node* n = m_buckets[i];
    while (n) {
      if (n->hash == h && m_eq(n->key, key)) return &n->value;
      n = n->next;
    }
    return nullptr;
  }

  const V* find(const K& key) const {
    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (m_cap - 1);

    Node* n = m_buckets[i];
    while (n) {
      if (n->hash == h && m_eq(n->key, key)) return &n->value;
      n = n->next;
    }
    return nullptr;
  }

  V& at(const K& key) {
    V* p = find(key);
    if (!p) panic("UnorderedMap::at: key not found");
    return *p;
  }

  const V& at(const K& key) const {
    const V* p = find(key);
    if (!p) panic("UnorderedMap::at: key not found");
    return *p;
  }

  bool contains(const K& key) const {
    return find(key) != nullptr;
  }

  void clear() {
    for (size_t i = 0; i < m_cap; ++i) {
      Node* n = m_buckets[i];
      m_buckets[i] = nullptr;
      while (n) {
        Node* next = n->next;
        n->~Node();
        m_freeNode(n);
        n = next;
      }
    }
    m_len = 0;
  }

  // ---------------------------------------------------------------------------
  // Rehash (buckets only, nodes stay in slabs)
  // ---------------------------------------------------------------------------
  void rehash(size_t new_cap) {
    new_cap = m_nextPow2(new_cap);
    const size_t old_cap = m_cap;

    Node** new_buckets = static_cast<Node**>(
        m_alloc->allocate(sizeof(Node*) * new_cap, alignof(Node*)));
    if (!new_buckets) panic("UnorderedMap::rehash: bucket allocation failed");

    for (size_t i = 0; i < new_cap; ++i) new_buckets[i] = nullptr;

    for (size_t i = 0; i < old_cap; ++i) {
      Node* n = m_buckets[i];
      while (n) {
        Node* next = n->next;
        const size_t idx = n->hash & (new_cap - 1);
        n->next = new_buckets[idx];
        new_buckets[idx] = n;
        n = next;
      }
    }

    if (m_can_free) m_alloc->free(m_buckets);
    m_buckets = new_buckets;
    m_cap = new_cap;
  }

  // ---------------------------------------------------------------------------
  // Iteration
  // ---------------------------------------------------------------------------
  class iterator {
  public:
    UnorderedMap* map;
    size_t bucket;
    Node* node;

    iterator(UnorderedMap* m, size_t b, Node* n)
        : map(m),
          bucket(b),
          node(n) {}

    auto operator*() const {
      return std::pair<const K&, V&>(node->key, node->value);
    }

    Node* operator->() const {
      return node;
    }

    iterator& operator++() {
      if (node) node = node->next;
      while (!node && ++bucket < map->m_cap) {
        node = map->m_buckets[bucket];
      }
      return *this;
    }

    bool operator==(const iterator& other) const {
      return node == other.node && bucket == other.bucket;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }
  };

  iterator begin() {
    for (size_t i = 0; i < m_cap; ++i)
      if (m_buckets[i]) return iterator(this, i, m_buckets[i]);
    return end();
  }

  iterator end() {
    return iterator(this, m_cap, nullptr);
  }

private:
  static size_t m_nextPow2(size_t x) {
    if (x == 0) return 1;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    if constexpr (sizeof(size_t) == 8) x |= x >> 32;
    return x + 1;
  }

  static size_t m_mixHash(size_t h) {
    h ^= (h >> 33);
    h *= 0xff51afd7ed558ccdULL;
    h ^= (h >> 33);
    return h;
  }

  inline Node* m_allocNode(size_t h, const K& key, const V& value) {
    if (!m_free) m_allocateSlab();

    Node* n = m_free;
    m_free = m_free->next;
    new (n) Node(h, key, value);
    n->next = nullptr;
    return n;
  }

  inline void m_freeNode(Node* n) noexcept {
    if (!n) return;
    n->next = m_free;
    m_free = n;
  }

  inline void m_allocateSlab() {
    void* raw = m_alloc->allocate(m_slab_bytes, alignof(Node));
    if (!raw) panic("UnorderedMap::allocate_slab: allocator failed");

    if (reinterpret_cast<uintptr_t>(raw) % alignof(Node) != 0) {
      panic(
          "UnorderedMap::allocate_slab: allocator returned unaligned pointer");
    }

    m_slabs.pushBack(raw);

    Node* base = static_cast<Node*>(raw);
    for (size_t i = 0; i < m_slab_size; ++i) {
      base[i].next = m_free;
      m_free = &base[i];
    }
  }
};
#pragma once

#include <functional>
#include <utility>
#include "allocators.hpp"

template <typename K, typename V>
class HashNode {
public:
  K key;
  V value;
  HashNode* next;

  HashNode(const K& k, const V& v)
      : key(k),
        value(v),
        next(nullptr) {}

  HashNode(K&& k, V&& v)
      : key(std::move(k)),
        value(std::move(v)),
        next(nullptr) {}
};

template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEq = std::equal_to<K>>
class UnorderedMap {
  // public member variables
public:
  class iterator;

  // private member variables
private:
  using Node = HashNode<K, V>;

  MemAllocator* m_alloc;
  size_t m_len;
  size_t m_cap;
  Node** m_buckets;
  Hash m_hash;
  KeyEq m_eq;

public:
  // constructors
  explicit UnorderedMap(MemAllocator& alloc, size_t cap = 16)
      : m_alloc(&alloc),
        m_len(0),
        m_cap(cap),
        m_buckets(static_cast<Node**>(
            alloc.allocate(sizeof(Node*) * cap, alignof(Node*)))) {
    for (size_t i = 0; i < cap; ++i) m_buckets[i] = nullptr;
  }

  ~UnorderedMap() {
    // nothing
  }

  UnorderedMap(const UnorderedMap&) = delete;
  UnorderedMap& operator=(const UnorderedMap&) = delete;

  UnorderedMap(UnorderedMap&& o) noexcept
      : m_alloc(o.m_alloc),
        m_len(o.m_len),
        m_cap(o.m_cap),
        m_buckets(o.m_buckets),
        m_hash(std::move(o.m_hash)),
        m_eq(std::move(o.m_eq)) {
    o.m_alloc = nullptr;
    o.m_buckets = nullptr;
    o.m_len = 0;
    o.m_cap = 0;
  }

  UnorderedMap& operator=(UnorderedMap&& o) noexcept {
    if (this == &o) return *this;

    m_alloc = o.m_alloc;
    m_len = o.m_len;
    m_cap = o.m_cap;
    m_buckets = o.m_buckets;
    m_hash = std::move(o.m_hash);
    m_eq = std::move(o.m_eq);

    o.m_alloc = nullptr;
    o.m_buckets = nullptr;
    o.m_len = 0;
    o.m_cap = 0;

    return *this;
  }

  // ---------------- REHASH ----------------
  void rehash(size_t new_cap) {
    size_t old_cap = m_cap;
    size_t old_bytes = sizeof(Node*) * old_cap;
    size_t new_bytes = sizeof(Node*) * new_cap;

    Node** new_buckets = nullptr;

    if (m_alloc->supportsResize()) {
      // Arena / Stack path
      new_buckets = static_cast<Node**>(
          m_alloc->resize(m_buckets, old_bytes, new_bytes, alignof(Node*)));

      // zero only the new slots
      for (size_t i = old_cap; i < new_cap; ++i) new_buckets[i] = nullptr;

      // in-place rehash
      for (size_t i = 0; i < old_cap; ++i) {
        Node* n = new_buckets[i];
        new_buckets[i] = nullptr;

        while (n) {
          Node* next = n->next;
          size_t idx = m_hash(n->key) % new_cap;
          n->next = new_buckets[idx];
          new_buckets[idx] = n;
          n = next;
        }
      }
    } else {
      // Pool / MallocAllocator path
      new_buckets =
          static_cast<Node**>(m_alloc->allocate(new_bytes, alignof(Node*)));

      for (size_t i = 0; i < new_cap; ++i) new_buckets[i] = nullptr;

      // rehash from old buckets
      for (size_t i = 0; i < old_cap; ++i) {
        Node* n = m_buckets[i];
        while (n) {
          Node* next = n->next;
          size_t idx = m_hash(n->key) % new_cap;
          n->next = new_buckets[idx];
          new_buckets[idx] = n;
          n = next;
        }
      }

      // free old bucket array
      m_alloc->free(m_buckets);
    }

    m_buckets = new_buckets;
    m_cap = new_cap;
  }

  // ---------------- operator[] ----------------
  V& operator[](const K& key) {
    if (m_loadFactor() > 0.75f) rehash(m_cap * 2);

    size_t i = m_idx(key);
    Node* n = m_buckets[i];

    while (n) {
      if (m_eq(n->key, key)) return n->value;
      n = n->next;
    }

    Node* newNode = m_makeNode(key, V{});
    newNode->next = m_buckets[i];
    m_buckets[i] = newNode;
    ++m_len;

    return newNode->value;
  }

  // ---------------- at ----------------
  V& at(const K& key) {
    size_t i = m_idx(key);
    Node* n = m_buckets[i];

    while (n) {
      if (m_eq(n->key, key)) return n->value;
      n = n->next;
    }

    panic("HashMap::at: key not found");
  }

  const V& at(const K& key) const {
    size_t i = m_idx(key);
    Node* n = m_buckets[i];

    while (n) {
      if (m_eq(n->key, key)) return n->value;
      n = n->next;
    }

    panic("HashMap::at: key not found");
  }

  // ---------------- contains ----------------
  bool contains(const K& key) const {
    size_t i = m_idx(key);
    Node* n = m_buckets[i];

    while (n) {
      if (m_eq(n->key, key)) return true;
      n = n->next;
    }
    return false;
  }

  // ---------------- insert_or_assign ----------------
  void insert(const K& key, const V& value) {
    if (m_loadFactor() > 0.75f) rehash(m_cap * 2);

    size_t i = m_idx(key);
    Node* n = m_buckets[i];

    while (n) {
      if (m_eq(n->key, key)) {
        n->value = value;
        return;
      }
      n = n->next;
    }

    Node* newNode = m_makeNode(key, value);
    newNode->next = m_buckets[i];
    m_buckets[i] = newNode;
    ++m_len;
  }

  // ---------------- erase ----------------
  bool erase(const K& key) {
    size_t i = m_idx(key);
    Node* prev = nullptr;
    Node* curr = m_buckets[i];

    while (curr) {
      if (m_eq(curr->key, key)) {
        if (prev)
          prev->next = curr->next;
        else
          m_buckets[i] = curr->next;

        if (m_alloc->getType() == AllocType::Pool) {
          curr->~Node();
          m_alloc->free(curr);
        }

        --m_len;
        return true;
      }
      prev = curr;
      curr = curr->next;
    }
    return false;
  }
  // ---------------- find ----------------
  V* find(const K& key) {
    size_t i = m_idx(key);
    Node* n = m_buckets[i];

    while (n) {
      if (m_eq(n->key, key)) return &n->value;
      n = n->next;
    }
    return nullptr;
  }

  const V* find(const K& key) const {
    size_t i = m_idx(key);
    Node* n = m_buckets[i];

    while (n) {
      if (m_eq(n->key, key)) return &n->value;
      n = n->next;
    }
    return nullptr;
  }

  // ---------------- clear ----------------
  void clear() {
    for (size_t i = 0; i < m_cap; ++i) {
      Node* n = m_buckets[i];
      m_buckets[i] = nullptr;

      while (n) {
        Node* next = n->next;

        // destroy value/key if needed
        n->~Node();

        // free only if allocator supports free()
        if (m_alloc->getType() == AllocType::Pool) m_alloc->free(n);

        n = next;
      }
    }

    m_len = 0;
  }

  // ---------------- iteration ----------------
  iterator begin() {
    for (size_t i = 0; i < m_cap; ++i) {
      if (m_buckets[i]) return iterator(this, i, m_buckets[i]);
    }
    return end();
  }

  iterator end() {
    return iterator(this, m_cap, nullptr);
  }

  // ---------------- misc ----------------
  size_t len() const {
    return m_len;
  }
  size_t cap() const {
    return m_cap;
  }

  // private functions
private:
  size_t m_idx(const K& key) const {
    return m_hash(key) % m_cap;
  }

  Node* m_makeNode(const K& key, const V& value) {
    void* mem = m_alloc->allocate(sizeof(Node), alignof(Node));
    return new (mem) Node(key, value);
  }

  float m_loadFactor() const {
    return static_cast<float>(m_len) / static_cast<float>(m_cap);
  }
};

// ================= ITERATOR =================

template <typename K, typename V, typename Hash, typename KeyEq>
class UnorderedMap<K, V, Hash, KeyEq>::iterator {
public:
  using Node = typename UnorderedMap::Node;

  UnorderedMap* map;
  size_t bucket;
  Node* node;

  iterator(UnorderedMap* m, size_t b, Node* n)
      : map(m),
        bucket(b),
        node(n) {}

  auto operator*() const {
    return std::pair<const K&, V&>(node->key, node->value);
  }

  Node* operator->() const {
    return node;
  }

  iterator& operator++() {
    if (node) node = node->next;

    while (!node && ++bucket < map->m_cap) node = map->m_buckets[bucket];

    return *this;
  }

  bool operator==(const iterator& other) const {
    return node == other.node && bucket == other.bucket;
  }

  bool operator!=(const iterator& other) const {
    return !(*this == other);
  }
};
