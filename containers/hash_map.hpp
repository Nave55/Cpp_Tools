#pragma once

#include <optional>
#include "allocators.hpp"
#include "vec.hpp"

// Node
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

// HashMap - (Arena/Stack/Pool)
template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEq = std::equal_to<K>>
class HashMap {
  using Node = HashNode<K, V>;

public:
  class Iterator;
  class ConstIterator;
  size_t len = 0;         // amount of entries in hashmap
  size_t cap = 0;         // total node capacity
  size_t slab_bytes = 0;  // bytes per slab
  size_t slab_size;       // nodes per slab
  size_t bucket_amt;      // number of buckets

private:
  Hash m_hash = Hash{};
  KeyEq m_eq = KeyEq{};
  Node* m_free = nullptr;  // free list head
  MemAllocator* m_alloc;   // custom allocator
  Node** m_buckets;
  bool m_can_free;     // if allocator support freeing data
  Vec<void*> m_slabs;  // raw slab pointers

public:
  explicit HashMap(MemAllocator& alloc = arena_alloc, size_t slab_size = 32,
                   size_t slabs = 4, size_t bucket_amt = 64)
      : slab_size(slab_size),
        bucket_amt(m_nextPow2(bucket_amt)),
        m_alloc(&alloc),
        m_buckets(static_cast<Node**>(
            alloc.allocate(sizeof(Node*) * this->bucket_amt, alignof(Node*)))),
        m_can_free(alloc.getType() == AllocType::Pool),
        m_slabs(0, slabs, alloc) {
    if (!m_buckets) panic("UnorderedMap: bucket allocation failed");
    for (size_t i = 0; i < this->bucket_amt; ++i) m_buckets[i] = nullptr;

    if (sizeof(Node) % alignof(Node) != 0)
      panic("UnorderedMap: Node size must be multiple of alignment");

    slab_bytes = slab_size * slabs;
  }

  explicit HashMap(std::initializer_list<std::pair<K, V>> init,
                   MemAllocator& alloc = arena_alloc, size_t slab_size = 32,
                   size_t slabs = 4, size_t bucket_amt = 64)
      : HashMap(alloc, slab_size, slabs, bucket_amt) {
    for (auto& [k, v] : init) insert(k, v);
    slab_bytes = slab_size * slabs;
  }

  ~HashMap() {
    clear();

    if (m_can_free) {
      m_alloc->free(m_buckets);
      for (size_t i = 0; i < m_slabs.len; ++i) {
        auto val = m_slabs.at(i);
        if (val.has_value()) m_alloc->free(static_cast<void*>(val.value()));
      }
    }
  }

  HashMap(const HashMap& o)
      : len(0),
        cap(o.cap),
        slab_bytes(o.slab_bytes),
        slab_size(o.slab_size),
        bucket_amt(o.bucket_amt),
        m_hash(o.m_hash),
        m_eq(o.m_eq),
        m_free(nullptr),
        m_alloc(o.m_alloc),
        m_buckets(static_cast<Node**>(
            m_alloc->allocate(sizeof(Node*) * bucket_amt, alignof(Node*)))),
        m_can_free(o.m_can_free),
        m_slabs(0, o.m_slabs.cap, *m_alloc) {
    if (!m_buckets) panic("HashMap copy: bucket allocation failed");
    for (size_t i = 0; i < bucket_amt; ++i) m_buckets[i] = nullptr;

    for (size_t i = 0; i < o.m_slabs.len; ++i) {
      void* slab = m_alloc->allocate(slab_bytes, alignof(Node));
      if (!slab) panic("HashMap copy: slab allocation failed");
      m_slabs.emplaceBack(slab);
    }

    for (size_t i = 0; i < o.bucket_amt; ++i) {
      Node* cur = o.m_buckets[i];
      while (cur) {
        insert(cur->key, cur->value);
        cur = cur->next;
      }
    }
  }

  HashMap& operator=(const HashMap& o) noexcept {
    if (this == &o) return *this;

    if (m_can_free) {
      if (m_buckets) m_alloc->free(m_buckets);

      for (size_t i = 0; i < m_slabs.len; ++i) {
        void* slab = m_slabs.at(i).value();
        m_alloc->free(slab);
      }
    }

    len = 0;
    cap = o.cap;
    slab_bytes = o.slab_bytes;
    slab_size = o.slab_size;
    bucket_amt = o.bucket_amt;
    m_hash = o.m_hash;
    m_eq = o.m_eq;
    m_free = nullptr;
    m_alloc = o.m_alloc;
    m_can_free = o.m_can_free;

    m_buckets = static_cast<Node**>(
        m_alloc->allocate(sizeof(Node*) * bucket_amt, alignof(Node*)));
    if (!m_buckets) panic("HashMap copy assignment: bucket allocation failed");

    for (size_t i = 0; i < bucket_amt; ++i) {
      m_buckets[i] = nullptr;
    }

    m_slabs.clear();

    for (size_t i = 0; i < o.m_slabs.len; ++i) {
      void* slab = m_alloc->allocate(slab_bytes, alignof(Node));
      if (!slab) panic("HashMap copy assignment: slab allocation failed");
      m_slabs.emplaceBack(slab);
    }

    for (size_t i = 0; i < o.bucket_amt; ++i) {
      Node* cur = o.m_buckets[i];
      while (cur) {
        insert(cur->key, cur->value);
        cur = cur->next;
      }
    }

    return *this;
  }

  HashMap(HashMap&& o) noexcept
      : m_alloc(o.m_alloc),
        len(o.len),
        bucket_amt(o.bucket_amt),
        m_buckets(o.m_buckets),
        m_hash(std::move(o.m_hash)),
        m_eq(std::move(o.m_eq)),
        m_can_free(o.m_can_free),
        m_slabs(std::move(o.m_slabs)),
        m_free(o.m_free),
        slab_size(o.slab_size),
        slab_bytes(o.slab_bytes) {
    o.m_alloc = nullptr;
    o.m_buckets = nullptr;
    o.len = 0;
    o.bucket_amt = 0;
    o.m_can_free = false;
    o.m_free = nullptr;
    o.slab_size = 0;
    o.slab_bytes = 0;
  }

  HashMap& operator=(HashMap&& o) noexcept {
    if (this == &o) return *this;
    this->~HashMap();
    new (this) HashMap(std::move(o));
    return *this;
  }

  V* operator[](const K& key) {
    return find(key);
  }

  const V* operator[](const K& key) const {
    return find(key);
  }

  std::optional<V&> at(const K& key) {
    V* p = find(key);
    if (!p) return std::nullopt;
    return *p;
  }

  std::optional<const V&> at(const K& key) const {
    const V* p = find(key);
    if (!p) return std::nullopt;
    return *p;
  }

  V& atOrInsert(const K& key) {
    if (len * 4 > bucket_amt * 3) rehash(bucket_amt * 2);

    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (bucket_amt - 1);

    Node* n = m_buckets[i];
    while (n) {
      if (n->hash == h && m_eq(n->key, key)) return n->value;
      n = n->next;
    }

    Node* newNode = m_allocNode(h, key, V{});
    newNode->next = m_buckets[i];
    m_buckets[i] = newNode;
    ++len;
    return newNode->value;
  }

  void print() const noexcept {
    std::printf("HashMap {\n");
    for (auto&& [k, v] : *this) {
      std::printf("  ");
      printValue(k);
      std::printf(" => ");
      printValue(v);
      std::printf("\n");
    }
    std::printf("}\n");
  }

  size_t remaining_capacity() const {
    return cap - len;
  }

  void insert(const K& key, const V& value) {
    if (len * 4 > bucket_amt * 3) rehash(bucket_amt * 2);

    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (bucket_amt - 1);

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
    ++len;
  }

  bool erase(const K& key) {
    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (bucket_amt - 1);

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
        --len;
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
    const size_t i = h & (bucket_amt - 1);

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
    const size_t i = h & (bucket_amt - 1);

    Node* n = m_buckets[i];
    while (n) {
      if (n->hash == h && m_eq(n->key, key)) return &n->value;
      n = n->next;
    }
    return nullptr;
  }

  bool contains(const K& key) const {
    return find(key) != nullptr;
  }

  void clear() {
    for (size_t i = 0; i < bucket_amt; ++i) {
      Node* n = m_buckets[i];
      m_buckets[i] = nullptr;
      while (n) {
        Node* next = n->next;
        n->~Node();
        m_freeNode(n);
        n = next;
      }
    }
    len = 0;
  }

  void rehash(size_t new_cap) {
    new_cap = m_nextPow2(new_cap);
    const size_t old_cap = bucket_amt;

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
    bucket_amt = new_cap;
  }

  class Iterator {
  public:
    HashMap* map;
    size_t bucket;
    Node* node;

  public:
    Iterator(HashMap* m, size_t b, Node* n)
        : map(m),
          bucket(b),
          node(n) {}

    auto operator*() const {
      return std::pair<const K&, V&>(node->key, node->value);
    }

    Node* operator->() const {
      return node;
    }

    Iterator& operator++() {
      if (node) node = node->next;
      while (!node && ++bucket < map->bucket_amt) {
        node = map->m_buckets[bucket];
      }
      return *this;
    }

    bool operator==(const Iterator& other) const {
      return node == other.node && bucket == other.bucket;
    }

    bool operator!=(const Iterator& other) const {
      return !(*this == other);
    }
  };

  class ConstIterator {
  public:
    const HashMap* map;
    size_t bucket;
    const Node* node;

  public:
    ConstIterator(const HashMap* m, size_t b, const Node* n)
        : map(m),
          bucket(b),
          node(n) {}

    auto operator*() const {
      return std::pair<const K&, const V&>(node->key, node->value);
    }

    const Node* operator->() const {
      return node;
    }

    ConstIterator& operator++() {
      if (node) node = node->next;
      while (!node && ++bucket < map->bucket_amt) node = map->m_buckets[bucket];
      return *this;
    }

    bool operator==(const ConstIterator& other) const {
      return node == other.node && bucket == other.bucket;
    }

    bool operator!=(const ConstIterator& other) const {
      return !(*this == other);
    }
  };

  Iterator begin() {
    for (size_t i = 0; i < bucket_amt; ++i)
      if (m_buckets[i]) return Iterator(this, i, m_buckets[i]);
    return end();
  }

  Iterator end() {
    return Iterator(this, bucket_amt, nullptr);
  }

  ConstIterator begin() const {
    for (size_t i = 0; i < bucket_amt; ++i)
      if (m_buckets[i]) return ConstIterator(this, i, m_buckets[i]);
    return end();
  }

  ConstIterator end() const {
    return ConstIterator(this, bucket_amt, nullptr);
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

  Node* m_allocNode(size_t h, const K& key, const V& value) {
    if (!m_free) m_allocateSlab();

    Node* n = m_free;
    m_free = m_free->next;
    new (n) Node(h, key, value);
    n->next = nullptr;
    return n;
  }

  void m_freeNode(Node* n) noexcept {
    if (!n) return;
    n->next = m_free;
    m_free = n;
  }

  void m_allocateSlab() {
    const size_t node_size = sizeof(Node);

    size_t nodes;
    if (len < 128)
      nodes = slab_size;
    else
      nodes = len / 4;

    nodes = std::max(nodes, slab_size);
    nodes = std::min(nodes, size_t(4096));

    // If backing allocator is Pool, cap by chunk capacity
    if (m_alloc->getType() == AllocType::Pool) {
      const size_t chunk = m_alloc->getChunkSize();
      const size_t max_nodes = chunk / node_size;
      if (max_nodes == 0)
        panic("UnorderedMap::allocate_slab: Pool chunk too small for Node");

      nodes = std::min(nodes, max_nodes);
    }

    slab_size = nodes;
    slab_bytes = nodes * node_size;

    void* raw = m_alloc->allocate(slab_bytes, alignof(Node));
    if (!raw) panic("UnorderedMap::allocate_slab: allocator failed");

    m_slabs.pushBack(raw);
    cap += nodes;

    Node* base = static_cast<Node*>(raw);
    for (size_t i = 0; i < slab_size; ++i) {
      base[i].next = m_free;
      m_free = &base[i];
    }
  }
};
