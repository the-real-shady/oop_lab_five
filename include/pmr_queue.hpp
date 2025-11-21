#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T>
class PmrQueue {
    struct Node {
        template <typename... Args>
        explicit Node(Args&&... args)
            : value(std::forward<Args>(args)...) {}

        T value;
        Node* next{nullptr};
    };

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

    explicit PmrQueue(std::pmr::memory_resource* resource = std::pmr::get_default_resource())
        : allocator_(resource) {}

    PmrQueue(const PmrQueue& other)
        : allocator_(other.allocator_.resource()) {
        Node* current = other.head_;
        while (current != nullptr) {
            push(current->value);
            current = current->next;
        }
    }

    PmrQueue(const PmrQueue& other, std::pmr::memory_resource* resource)
        : allocator_(resource) {
        Node* current = other.head_;
        while (current != nullptr) {
            push(current->value);
            current = current->next;
        }
    }

    PmrQueue(PmrQueue&& other) noexcept
        : allocator_(other.allocator_.resource()),
          head_(std::exchange(other.head_, nullptr)),
          tail_(std::exchange(other.tail_, nullptr)),
          size_(std::exchange(other.size_, 0)) {}

    ~PmrQueue() { clear(); }

    PmrQueue& operator=(const PmrQueue& other) {
        if (this == &other) {
            return *this;
        }

        PmrQueue copy(other, allocator_.resource());
        swap(copy);
        return *this;
    }

    PmrQueue& operator=(PmrQueue&& other) noexcept {
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

    void push(const T& value) { emplace(value); }
    void push(T&& value) { emplace(std::move(value)); }

    template <typename... Args>
    T& emplace(Args&&... args) {
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

    void pop() {
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

    void clear() noexcept {
        while (head_ != nullptr) {
            Node* node = head_;
            head_ = head_->next;
            destroy_node(node);
        }
        tail_ = nullptr;
        size_ = 0;
    }

    T& front() {
        if (empty()) {
            throw std::runtime_error("front() called on empty PmrQueue");
        }
        return head_->value;
    }

    const T& front() const {
        if (empty()) {
            throw std::runtime_error("front() called on empty PmrQueue");
        }
        return head_->value;
    }

    T& back() {
        if (empty()) {
            throw std::runtime_error("back() called on empty PmrQueue");
        }
        return tail_->value;
    }

    const T& back() const {
        if (empty()) {
            throw std::runtime_error("back() called on empty PmrQueue");
        }
        return tail_->value;
    }

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }

    iterator begin() noexcept { return iterator(head_); }
    iterator end() noexcept { return iterator(nullptr); }

    void swap(PmrQueue& other) noexcept {
        using std::swap;
        swap(allocator_, other.allocator_);
        swap(head_, other.head_);
        swap(tail_, other.tail_);
        swap(size_, other.size_);
    }

    std::pmr::memory_resource* resource() const noexcept {
        return allocator_.resource();
    }

private:
    template <typename... Args>
    Node* create_node(Args&&... args) {
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

    void destroy_node(Node* node) noexcept {
        if (node == nullptr) {
            return;
        }

        std::allocator_traits<allocator_type>::destroy(allocator_, node);
        allocator_.deallocate(node, 1);
    }

    allocator_type allocator_;
    Node* head_{nullptr};
    Node* tail_{nullptr};
    size_type size_{0};
};
