#pragma once

#include <cstddef>
#include <iterator>
#include <memory_resource>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T>
class PmrQueue {
    struct Node;

public:
    using value_type = T;
    using allocator_type = std::pmr::polymorphic_allocator<Node>;
    using size_type = std::size_t;

    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator() noexcept = default;

        reference operator*() const { return current_->value; }
        pointer operator->() const { return &current_->value; }

        iterator& operator++() {
            if (current_ != nullptr) {
                current_ = current_->next;
            }
            return *this;
        }

        iterator operator++(int) {
            iterator copy = *this;
            ++(*this);
            return copy;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs) {
            return lhs.current_ == rhs.current_;
        }

        friend bool operator!=(const iterator& lhs, const iterator& rhs) {
            return !(lhs == rhs);
        }

    private:
        friend class PmrQueue<T>;
        explicit iterator(Node* node) : current_(node) {}
        Node* current_{nullptr};
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = const T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator() noexcept = default;
        const_iterator(const iterator& it) : current_(it.current_) {}

        reference operator*() const { return current_->value; }
        pointer operator->() const { return &current_->value; }

        const_iterator& operator++() {
            if (current_ != nullptr) {
                current_ = current_->next;
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator copy = *this;
            ++(*this);
            return copy;
        }

        friend bool operator==(const const_iterator& lhs, const const_iterator& rhs) {
            return lhs.current_ == rhs.current_;
        }

        friend bool operator!=(const const_iterator& lhs, const const_iterator& rhs) {
            return !(lhs == rhs);
        }

    private:
        friend class PmrQueue<T>;
        explicit const_iterator(const Node* node) : current_(node) {}
        const Node* current_{nullptr};
    };

    explicit PmrQueue(std::pmr::memory_resource* resource = std::pmr::get_default_resource());
    PmrQueue(const PmrQueue& other);
    PmrQueue(const PmrQueue& other, std::pmr::memory_resource* resource);
    PmrQueue(PmrQueue&& other) noexcept;
    ~PmrQueue();

    PmrQueue& operator=(const PmrQueue& other);
    PmrQueue& operator=(PmrQueue&& other) noexcept;

    void push(const T& value);
    void push(T&& value);

    template <typename... Args>
    T& emplace(Args&&... args);

    void pop();
    void clear() noexcept;

    T& front();
    const T& front() const;

    T& back();
    const T& back() const;

    bool empty() const noexcept;
    size_type size() const noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    void swap(PmrQueue& other) noexcept;
    std::pmr::memory_resource* resource() const noexcept;

private:
    struct Node {
        template <typename... Args>
        explicit Node(Args&&... args)
            : value(std::forward<Args>(args)...) {}

        T value;
        Node* next{nullptr};
    };

    template <typename... Args>
    Node* create_node(Args&&... args);

    void destroy_node(Node* node) noexcept;

    allocator_type allocator_;
    Node* head_{nullptr};
    Node* tail_{nullptr};
    size_type size_{0};
};

template <typename T>
PmrQueue<T>::PmrQueue(std::pmr::memory_resource* resource)
    : allocator_(resource) {}

template <typename T>
PmrQueue<T>::PmrQueue(const PmrQueue& other)
    : allocator_(other.allocator_.resource()) {
    for (const auto& value : other) {
        push(value);
    }
}

template <typename T>
PmrQueue<T>::PmrQueue(const PmrQueue& other, std::pmr::memory_resource* resource)
    : allocator_(resource) {
    for (const auto& value : other) {
        push(value);
    }
}

template <typename T>
PmrQueue<T>::PmrQueue(PmrQueue&& other) noexcept
    : allocator_(other.allocator_.resource()),
      head_(std::exchange(other.head_, nullptr)),
      tail_(std::exchange(other.tail_, nullptr)),
      size_(std::exchange(other.size_, 0)) {}

template <typename T>
PmrQueue<T>::~PmrQueue() {
    clear();
}

template <typename T>
PmrQueue<T>& PmrQueue<T>::operator=(const PmrQueue& other) {
    if (this == &other) {
        return *this;
    }

    PmrQueue copy(other, allocator_.resource());
    swap(copy);
    return *this;
}

template <typename T>
PmrQueue<T>& PmrQueue<T>::operator=(PmrQueue&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    clear();
    allocator_ = allocator_type(other.allocator_.resource());
    head_ = std::exchange(other.head_, nullptr);
    tail_ = std::exchange(other.tail_, nullptr);
    size_ = std::exchange(other.size_, 0);
    return *this;
}

template <typename T>
void PmrQueue<T>::push(const T& value) {
    emplace(value);
}

template <typename T>
void PmrQueue<T>::push(T&& value) {
    emplace(std::move(value));
}

template <typename T>
template <typename... Args>
T& PmrQueue<T>::emplace(Args&&... args) {
    Node* node = create_node(std::forward<Args>(args)...);
    if (tail_ != nullptr) {
        tail_->next = node;
    } else {
        head_ = node;
    }
    tail_ = node;
    ++size_;
    return tail_->value;
}

template <typename T>
void PmrQueue<T>::pop() {
    if (empty()) {
        throw std::runtime_error("Attempt to pop from an empty PmrQueue");
    }

    Node* old_head = head_;
    head_ = head_->next;
    if (head_ == nullptr) {
        tail_ = nullptr;
    }

    destroy_node(old_head);
    --size_;
}

template <typename T>
void PmrQueue<T>::clear() noexcept {
    while (head_ != nullptr) {
        Node* node = head_;
        head_ = head_->next;
        destroy_node(node);
    }
    tail_ = nullptr;
    size_ = 0;
}

template <typename T>
T& PmrQueue<T>::front() {
    if (empty()) {
        throw std::runtime_error("front() called on empty PmrQueue");
    }
    return head_->value;
}

template <typename T>
const T& PmrQueue<T>::front() const {
    if (empty()) {
        throw std::runtime_error("front() called on empty PmrQueue");
    }
    return head_->value;
}

template <typename T>
T& PmrQueue<T>::back() {
    if (empty()) {
        throw std::runtime_error("back() called on empty PmrQueue");
    }
    return tail_->value;
}

template <typename T>
const T& PmrQueue<T>::back() const {
    if (empty()) {
        throw std::runtime_error("back() called on empty PmrQueue");
    }
    return tail_->value;
}

template <typename T>
bool PmrQueue<T>::empty() const noexcept {
    return size_ == 0;
}

template <typename T>
typename PmrQueue<T>::size_type PmrQueue<T>::size() const noexcept {
    return size_;
}

template <typename T>
typename PmrQueue<T>::iterator PmrQueue<T>::begin() noexcept {
    return iterator(head_);
}

template <typename T>
typename PmrQueue<T>::iterator PmrQueue<T>::end() noexcept {
    return iterator(nullptr);
}

template <typename T>
typename PmrQueue<T>::const_iterator PmrQueue<T>::begin() const noexcept {
    return const_iterator(head_);
}

template <typename T>
typename PmrQueue<T>::const_iterator PmrQueue<T>::end() const noexcept {
    return const_iterator(nullptr);
}

template <typename T>
typename PmrQueue<T>::const_iterator PmrQueue<T>::cbegin() const noexcept {
    return begin();
}

template <typename T>
typename PmrQueue<T>::const_iterator PmrQueue<T>::cend() const noexcept {
    return end();
}

template <typename T>
void PmrQueue<T>::swap(PmrQueue& other) noexcept {
    using std::swap;
    swap(allocator_, other.allocator_);
    swap(head_, other.head_);
    swap(tail_, other.tail_);
    swap(size_, other.size_);
}

template <typename T>
std::pmr::memory_resource* PmrQueue<T>::resource() const noexcept {
    return allocator_.resource();
}

template <typename T>
template <typename... Args>
typename PmrQueue<T>::Node* PmrQueue<T>::create_node(Args&&... args) {
    Node* node = allocator_.allocate(1);
    try {
        std::allocator_traits<allocator_type>::construct(
            allocator_,
            node,
            std::forward<Args>(args)...);
    } catch (...) {
        allocator_.deallocate(node, 1);
        throw;
    }
    return node;
}

template <typename T>
void PmrQueue<T>::destroy_node(Node* node) noexcept {
    if (node == nullptr) {
        return;
    }

    std::allocator_traits<allocator_type>::destroy(allocator_, node);
    allocator_.deallocate(node, 1);
}
