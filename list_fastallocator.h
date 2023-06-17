#include <vector>
#include <iostream>

template<size_t chunkSize>
class FixedAllocator {
 private:
  struct Pool {
    int8_t data[chunkSize];
  };

  size_t cur_pos;
  std::vector<Pool *> pools;
  std::vector<Pool *> reused;

  size_t size = 2048;

  void new_pool() {
    auto pool = new Pool[size];
    pools.push_back(pool);
    cur_pos = 0;
  }

  FixedAllocator() {
    new_pool();
  }

 public:

  FixedAllocator(const FixedAllocator &) = delete;

  static FixedAllocator &get_instance() {
    static FixedAllocator alloc;
    return alloc;
  }

  void *allocate() {
    Pool *ptr;
    if (!reused.empty()) {
      ptr = reused[reused.size() - 1];
      reused.pop_back();
    } else {
      if (cur_pos == size) {
        new_pool();
      }
      ptr = &pools[pools.size() - 1][cur_pos];
      cur_pos++;
    }
    return static_cast<void *>(ptr);
  }

  void deallocate(void *ptr) {
    Pool *reused_pool = static_cast<Pool *>(ptr);
    reused.push_back(reused_pool);
  }

  ~FixedAllocator() {
    for (auto pl : pools) {
      delete[] pl;
    }
  }

};

template<typename T>
class FastAllocator {
  static size_t counter;

 public:
  FastAllocator() = default;

  FastAllocator(const FastAllocator<T> &) = default;

  template<typename U>
  FastAllocator(const FastAllocator<U> &) {}

  using value_type = T;

  T *allocate(size_t count) {
    if (sizeof(T) * count == 4) {
      return static_cast<T *>(FixedAllocator<4>::get_instance().allocate());
    } else if (sizeof(T) * count == 8) {
      return static_cast<T *>(FixedAllocator<8>::get_instance().allocate());
    } else if (sizeof(T) * count == 16) {
      return static_cast<T *>(FixedAllocator<16>::get_instance().allocate());
    } else if (sizeof(T) * count == 20) {
      return static_cast<T *>(FixedAllocator<20>::get_instance().allocate());
    } else if (sizeof(T) * count == 24) {
      return static_cast<T *>(FixedAllocator<24>::get_instance().allocate());
    } else {
      return std::allocator<T>().allocate(count);
    }
  }

  void deallocate(T *ptr, size_t count) {
    if (sizeof(T) * count == 4) {
      FixedAllocator<4>::get_instance().deallocate(ptr);
    } else if (sizeof(T) * count == 8) {
      FixedAllocator<8>::get_instance().deallocate(ptr);
    } else if (sizeof(T) * count == 16) {
      FixedAllocator<16>::get_instance().deallocate(ptr);
    } else if (sizeof(T) * count == 20) {
      FixedAllocator<20>::get_instance().deallocate(ptr);
    }else if (sizeof(T) * count == 24) {
      FixedAllocator<24>::get_instance().deallocate(ptr);
    } else {
      std::allocator<T>().deallocate(ptr, count);
    }
  }
};


template<typename T, typename Allocator = std::allocator<T>>
class List {
 private:
  struct Node {
    T value;
    Node *next;
    Node *prev;

    Node() = default;

    Node(const T &val) : value(val) {}

    Node(const T &val, Node *p, Node *n) : value(val), next(n), prev(p) {}

    Node(Node *p, Node *n) : next(n), prev(p) {}
  };

  void destroy();

  size_t _size = 0;
  Node *fake;
  Allocator allocc;
  using traits = typename std::allocator_traits<typename std::allocator_traits<Allocator>::template rebind_alloc<Node>>;
  using altraits = typename std::allocator_traits<Allocator>;
  typename std::allocator_traits<Allocator>::template rebind_alloc<Node> allocator;

 public:
  explicit List(const Allocator &alloc = Allocator()) : allocc(alloc), allocator(alloc) {
    fake = allocator.allocate(1);
    fake->next = fake;
    fake->prev = fake;
  }

  List(size_t count, const T &value, const Allocator &alloc = Allocator()) : List(alloc) {
    for (size_t it = 0; it < count; ++it) {
      push_back(value);
    }
  }

  List(size_t count, const Allocator &alloc = Allocator()) : List(alloc) {
    auto prev = fake;
    for (size_t it = 0; it < count; ++it) {
      auto node = traits::allocate(allocator, 1);
      traits::construct(allocator, node, nullptr, nullptr);
      node->prev = prev;
      prev->next = node;
      prev = node;
      _size++;
    }
    prev->next = fake;
    fake->prev = prev;
  }

  List(const List &another) : List(altraits::select_on_container_copy_construction(another.allocc)) {
    for (auto it = another.begin(); it != another.end(); ++it) {
      push_back(*it);
    }
  }

  ~List() {
    destroy();
  }

  List &operator=(const List &another) {
    if (&another == this) {
      return *this;
    }
    destroy();
    if (traits::propagate_on_container_copy_assignment::value) {
      allocc = another.allocc;
      allocator = another.allocator;
    }
    fake = allocator.allocate(1);
    fake->next = fake;
    fake->prev = fake;
    for (auto it = another.begin(); it != another.end(); ++it) {
      push_back(*it);
    }
    return *this;
  }

  size_t size() const {
    return _size;
  }

  void push_back(const T &val) {
    insert(end(), val);
  }

  void push_front(const T &val) {
    insert(begin(), val);
  }

  void pop_back() {
    auto it = end();
    --it;
    erase(it);
  }

  void pop_front() {
    erase(begin());
  }

 private:
  template<bool IsConst>
  class common_iterator
          : public std::iterator<std::bidirectional_iterator_tag, std::conditional_t<IsConst, const T, T>> {
    friend class List<T, Allocator>;

    Node *ptr;

   public:

    operator common_iterator<true>() {
      return common_iterator<true>(ptr);
    }


    common_iterator(Node *pointer) {
      ptr = pointer;
    }

    common_iterator<IsConst> &operator=(const common_iterator<IsConst> &another) {
      ptr = another.ptr;
      return *this;
    }

    typename std::conditional<IsConst, const T &, T &>::type operator*() const {
      return ptr->value;
    }

    typename std::conditional<IsConst, const T *, T *>::type operator->() const {
      return &(ptr->value);
    }

    common_iterator<IsConst> &operator++() {
      ptr = ptr->next;
      return *this;
    }

    common_iterator<IsConst> operator++(int) {
      auto copy = *this;
      ++(*this);
      return copy;
    }

    common_iterator<IsConst> &operator--() {
      ptr = ptr->prev;
      return *this;
    }

    common_iterator<IsConst> operator--(int) {
      auto copy = *this;
      --(*this);
      return copy;
    }

    bool operator==(const common_iterator<IsConst> &another) const {
      return ptr == another.ptr;
    }

    bool operator!=(const common_iterator<IsConst> &another) const {
      return ptr != another.ptr;
    }
  };

 public:

  using iterator = common_iterator<false>;
  using const_iterator = common_iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() {
    return iterator(fake->next);
  }

  const_iterator begin() const {
    return const_iterator(fake->next);
  }

  const_iterator cbegin() const {
    return const_iterator(fake->next);
  }

  reverse_iterator rbegin() {
    return reverse_iterator(fake);
  }

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(fake);
  }

  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(fake);
  }

  iterator end() {
    return iterator(fake);
  }

  const_iterator end() const {
    return const_iterator(fake);
  }

  const_iterator cend() const {
    return const_iterator(fake);
  }

  reverse_iterator rend() {
    return reverse_iterator(fake->next);
  }

  const_reverse_iterator rend() const {
    return const_reverse_iterator(fake->next);
  }

  const_reverse_iterator crend() const {
    return const_reverse_iterator(fake->next);
  }

  Allocator get_allocator() const {
    return allocc;
  }

  void insert(const_iterator it, const T &val) {
    auto ptr = it.ptr;
    auto new_node = traits::allocate(allocator, 1);
    traits::construct(allocator, new_node, val, ptr->prev, ptr);
    ptr->prev->next = new_node;
    ptr->prev = new_node;
    ++_size;
  }

  void erase(const_iterator it) {
    auto ptr = it.ptr;
    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    traits::destroy(allocator, ptr);
    traits::deallocate(allocator, ptr, 1);
    --_size;
  }
};


template<typename T, typename Allocator>
void List<T, Allocator>::destroy() {
  if (fake == nullptr) return;
  size_t count = _size;
  for (size_t it = 0; it < count; ++it) {
    auto iter = end();
    --iter;
    erase(iter);
  }
  traits::deallocate(allocator, fake, 1);
}