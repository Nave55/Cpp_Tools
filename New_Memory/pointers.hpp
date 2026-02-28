#include <cstdint>
#include <iostream>

template <typename T>
class UniqPtr {
public:
  T* ptr;

public:
  UniqPtr()
      : ptr(nullptr) {}

  explicit UniqPtr(T* raw)
      : ptr{raw} {}

  explicit UniqPtr(T val)
      : ptr(new T(val)) {}

  UniqPtr(const UniqPtr&) = delete;

  UniqPtr& operator=(const UniqPtr&) = delete;

  UniqPtr(UniqPtr&& other) noexcept
      : ptr(other.ptr) {
    other.ptr = nullptr;
  }

  UniqPtr& operator=(UniqPtr&& other) noexcept {
    if (this != &other) {
      delete ptr;
      ptr = other.ptr;
      other.ptr = nullptr;
    }
    return *this;
  }

  ~UniqPtr() {
    delete (ptr);
  }
};

struct ControlBlock {
  std::atomic<uint32_t> strong;
  std::atomic<uint32_t> weak;
  void (*deleter)(void*);
  void* object;
};

template <typename T>
class SharedPtr {
public:
  T* ptr;
  ControlBlock* cb;

public:
  SharedPtr()
      : ptr(nullptr),
        cb(nullptr) {}

  explicit SharedPtr(T* raw)
      : ptr(raw) {
    cb =
        new ControlBlock{1, 1, [](void* p) { delete static_cast<T*>(p); }, raw};
  }

  template <typename... Args>
  explicit SharedPtr(Args&&... args)
      : ptr(new T(std::forward<Args>(args)...)) {
    cb =
        new ControlBlock{1, 1, [](void* p) { delete static_cast<T*>(p); }, ptr};
  }

  SharedPtr(const SharedPtr& other) noexcept
      : ptr(other.ptr),
        cb(other.cb) {
    cb->strong.fetch_add(1);
  }

  SharedPtr& operator=(const SharedPtr& other) noexcept {
    if (this != &other) {
      m_release();
      ptr = other.ptr;
      cb = other.cb;
      cb->strong.fetch_add(1);
    }
    return *this;
  }

  SharedPtr(SharedPtr&& other) noexcept
      : ptr(other.ptr),
        cb(other.cb) {
    other.ptr = nullptr;
    other.cb = nullptr;
  }

  SharedPtr& operator=(SharedPtr&& other) noexcept {
    if (this != &other) {
      m_release();
      ptr = other.ptr;
      cb = other.cb;
      other.ptr = nullptr;
      other.cb = nullptr;
    }
    return *this;
  }

  ~SharedPtr() {
    m_release();
  }

private:
  void m_release() {
    if (!cb) return;

    if (cb->strong.fetch_sub(1) == 1) {
      cb->deleter(cb->object);

      if (cb->weak.fetch_sub(1) == 1) {
        delete cb;
#ifdef DEBUG
        std::cout << "Shared Ptr Released\n";
#endif
      }
    }
  }
};

template <typename T>
class WeakPtr {
public:
  ControlBlock* cb;

public:
  WeakPtr()
      : cb(nullptr) {}

  explicit WeakPtr(const SharedPtr<T>& sp)
      : cb(sp.cb) {
    if (cb) cb->weak.fetch_add(1);
  }

  explicit WeakPtr(const WeakPtr& other)
      : cb(other.cb) {
    if (cb) cb->weak.fetch_add(1);
  }

  ~WeakPtr() {
    if (cb && cb->weak.fetch_sub(1) == 1) {
      delete cb;  // only if strong == 0 too
    }
  }

  SharedPtr<T> lock() const {
    if (!cb) return SharedPtr<T>();

    long count = cb->strong.load();
    if (count == 0) return SharedPtr<T>();  // object is gone

    // try to increment strong
    if (cb->strong.fetch_add(1) == 0) {
      // object died between load() and fetch_add()
      cb->strong.fetch_sub(1);
      return SharedPtr<T>();
    }

    // success: create a SharedPtr that shares ownership
    SharedPtr<T> sp;
    sp.cb = cb;
    sp.ptr = static_cast<T*>(cb->object);
    return sp;
  }
};

class RefCounted {
private:
  std::atomic<int> m_refcount;

public:
  void add_ref() noexcept {
    m_refcount.fetch_add(1, std::memory_order_relaxed);
  }

  void release_ref() noexcept {
    if (m_refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      delete this;
    }
  }

protected:
  RefCounted() noexcept
      : m_refcount(0) {}
  virtual ~RefCounted() = default;
};

template <typename T>
class IntrusivePtr {
private:
  T* m_ptr;

public:
  IntrusivePtr() noexcept
      : m_ptr(nullptr) {}

  explicit IntrusivePtr(T* p, bool add_ref = true)
      : m_ptr(p) {
    if (m_ptr && add_ref) {
      m_ptr->add_ref();
    }
  }

  IntrusivePtr(const IntrusivePtr& other) noexcept
      : m_ptr(other.m_ptr) {
    if (m_ptr) m_ptr->add_ref();
  }

  IntrusivePtr(IntrusivePtr&& other) noexcept
      : m_ptr(other.m_ptr) {
    other.m_ptr = nullptr;
  }

  ~IntrusivePtr() {
    if (m_ptr) {
      m_ptr->release_ref();
#ifdef DEBUG
      std::cout << "Intrusive Ptr Released\n";
#endif
    }
  }

  IntrusivePtr& operator=(const IntrusivePtr& other) noexcept {
    if (this != &other) {
      if (other.m_ptr) other.m_ptr->add_ref();
      if (m_ptr) m_ptr->release_ref();
      m_ptr = other.m_ptr;
    }
    return *this;
  }

  IntrusivePtr& operator=(IntrusivePtr&& other) noexcept {
    if (this != &other) {
      if (m_ptr) m_ptr->release_ref();
      m_ptr = other.m_ptr;
      other.m_ptr = nullptr;
    }
    return *this;
  }

  T* get() const noexcept {
    return m_ptr;
  }
  T& operator*() const noexcept {
    return *m_ptr;
  }
  T* operator->() const noexcept {
    return m_ptr;
  }
  explicit operator bool() const noexcept {
    return m_ptr != nullptr;
  }
};

