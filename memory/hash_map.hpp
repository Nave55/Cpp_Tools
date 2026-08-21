#pragma once

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
  size_t len;
  size_t cap;

private:
  MemAllocator* m_alloc;
  Node** m_buckets;
  Hash m_hash;
  KeyEq m_eq;
  bool m_can_free;  // only Pool frees

  Vec<void*> m_slabs;   // raw slab pointers
  Node* m_free;         // free list head
  size_t m_slab_size;   // nodes per slab
  size_t m_slab_bytes;  // bytes per slab

public:
  explicit HashMap(size_t cap = 16, MemAllocator& alloc = arena_alloc)
      : len(0),
        cap(m_nextPow2(cap)),
        m_alloc(&alloc),
        m_buckets(static_cast<Node**>(
            alloc.allocate(sizeof(Node*) * this->cap, alignof(Node*)))),
        m_hash(Hash{}),
        m_eq(KeyEq{}),
        m_can_free(alloc.getType() == AllocType::Pool),
        m_slabs(0, 4, alloc),
        m_free(nullptr),
        m_slab_size(0),
        m_slab_bytes(0) {
    if (!m_buckets) panic("UnorderedMap: bucket allocation failed");
    for (size_t i = 0; i < this->cap; ++i) m_buckets[i] = nullptr;

    static_assert(sizeof(Node) % alignof(Node) == 0,
                  "UnorderedMap: Node size must be multiple of alignment");
  }

  ~HashMap() {
    clear();

    if (m_can_free) {
      m_alloc->free(m_buckets);
      for (size_t i = 0; i < m_slabs.size(); ++i) {
        void* raw = m_slabs[i];
        if (raw) m_alloc->free(raw);
      }
    }
  }

  HashMap(const HashMap&) = delete;
  HashMap& operator=(const HashMap&) = delete;

  HashMap(HashMap&& o) noexcept
      : m_alloc(o.m_alloc),
        len(o.len),
        cap(o.cap),
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
    o.len = 0;
    o.cap = 0;
    o.m_can_free = false;
    o.m_free = nullptr;
    o.m_slab_size = 0;
    o.m_slab_bytes = 0;
  }

  HashMap& operator=(HashMap&& o) noexcept {
    if (this == &o) return *this;
    this->~HashMap();
    new (this) HashMap(std::move(o));
    return *this;
  }

  V& operator[](const K& key) {
    if (len * 4 > cap * 3) rehash(cap * 2);

    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (cap - 1);

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

  void insert(const K& key, const V& value) {
    if (len * 4 > cap * 3) rehash(cap * 2);

    const size_t raw = m_hash(key);
    const size_t h = m_mixHash(raw);
    const size_t i = h & (cap - 1);

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
    const size_t i = h & (cap - 1);

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
    const size_t i = h & (cap - 1);

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
    const size_t i = h & (cap - 1);

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
    for (size_t i = 0; i < cap; ++i) {
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
    const size_t old_cap = cap;

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
    cap = new_cap;
  }

  class Iterator {
  public:
    HashMap* map;
    size_t bucket;
    Node* node;

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
      while (!node && ++bucket < map->cap) {
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

  Iterator begin() {
    for (size_t i = 0; i < cap; ++i)
      if (m_buckets[i]) return Iterator(this, i, m_buckets[i]);
    return end();
  }

  Iterator end() {
    return Iterator(this, cap, nullptr);
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
    if (len < 128) {
      nodes = 64;
    } else {
      nodes = len / 4;
    }

    nodes = std::max(nodes, size_t(64));
    nodes = std::min(nodes, size_t(4096));

    m_slab_size = nodes;
    m_slab_bytes = nodes * node_size;

    void* raw = m_alloc->allocate(m_slab_bytes, alignof(Node));
    if (!raw) panic("UnorderedMap::allocate_slab: allocator failed");

    m_slabs.pushBack(raw);

    Node* base = static_cast<Node*>(raw);
    for (size_t i = 0; i < m_slab_size; ++i) {
      base[i].next = m_free;
      m_free = &base[i];
    }
  }
};
