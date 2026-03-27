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
  void insert_or_assign(const K& key, const V& value) {
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

        // Only destroy + free if allocator supports free()
        if (m_alloc->getType() == AllocType::Pool) {
          curr->~Node();
          m_alloc->free(curr);
        }

        // Otherwise: do NOT destroy the node.
        // It becomes unreachable and harmless.
        // Arena/Stack will reclaim it when reset.

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
