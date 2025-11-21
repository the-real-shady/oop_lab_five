#pragma once

#include <algorithm>
#include <cstddef>
#include <memory_resource>
#include <new>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

class DynamicTrackingResource : public std::pmr::memory_resource {
public:
    struct BlockSnapshot {
        std::size_t size{};
        std::size_t alignment{};
        bool in_use{};
    };

    explicit DynamicTrackingResource(std::string label = "dynamic-tracker");
    ~DynamicTrackingResource() override;

    DynamicTrackingResource(const DynamicTrackingResource&) = delete;
    DynamicTrackingResource& operator=(const DynamicTrackingResource&) = delete;

    std::size_t tracked_blocks() const;
    std::size_t active_blocks() const;
    std::size_t bytes_in_use() const;
    std::vector<BlockSnapshot> snapshot() const;
    const std::string& label() const noexcept;

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override;
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

private:
    struct Block {
        void* ptr{};
        std::size_t size{};
        std::size_t alignment{};
        bool in_use{};
    };

    std::vector<Block> blocks_;
    std::string label_;
};

inline DynamicTrackingResource::DynamicTrackingResource(std::string label)
    : label_(std::move(label)) {}

inline DynamicTrackingResource::~DynamicTrackingResource() {
    for (auto& block : blocks_) {
        if (block.ptr != nullptr) {
            ::operator delete(block.ptr, std::align_val_t(block.alignment));
            block.ptr = nullptr;
        }
    }
}

inline std::size_t DynamicTrackingResource::tracked_blocks() const {
    return blocks_.size();
}

inline std::size_t DynamicTrackingResource::active_blocks() const {
    return static_cast<std::size_t>(std::count_if(
        blocks_.begin(),
        blocks_.end(),
        [](const Block& block) { return block.in_use; }));
}

inline std::size_t DynamicTrackingResource::bytes_in_use() const {
    return std::accumulate(
        blocks_.begin(),
        blocks_.end(),
        static_cast<std::size_t>(0),
        [](std::size_t sum, const Block& block) {
            return block.in_use ? sum + block.size : sum;
        });
}

inline std::vector<DynamicTrackingResource::BlockSnapshot> DynamicTrackingResource::snapshot() const {
    std::vector<BlockSnapshot> info;
    info.reserve(blocks_.size());
    for (const auto& block : blocks_) {
        info.push_back(BlockSnapshot{
            .size = block.size,
            .alignment = block.alignment,
            .in_use = block.in_use,
        });
    }
    return info;
}

inline const std::string& DynamicTrackingResource::label() const noexcept {
    return label_;
}

inline void* DynamicTrackingResource::do_allocate(std::size_t bytes, std::size_t alignment) {
    if (bytes == 0) {
        bytes = 1;
    }

    auto finder = [&](const Block& block) {
        const bool alignment_ok = block.alignment >= alignment;
        return !block.in_use && alignment_ok && block.size >= bytes;
    };

    const auto reusable_block = std::find_if(blocks_.begin(), blocks_.end(), finder);
    if (reusable_block != blocks_.end()) {
        reusable_block->in_use = true;
        return reusable_block->ptr;
    }

    void* memory = ::operator new(bytes, std::align_val_t(alignment));
    blocks_.push_back(Block{
        .ptr = memory,
        .size = bytes,
        .alignment = alignment,
        .in_use = true,
    });
    return memory;
}

inline void DynamicTrackingResource::do_deallocate(void* p, std::size_t /*bytes*/, std::size_t /*alignment*/) {
    if (p == nullptr) {
        return;
    }

    const auto block = std::find_if(
        blocks_.begin(),
        blocks_.end(),
        [&](const Block& tracked) { return tracked.ptr == p; });

    if (block == blocks_.end()) {
        throw std::runtime_error("Attempted to release memory not owned by DynamicTrackingResource");
    }

    block->in_use = false;
}

inline bool DynamicTrackingResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept {
    return this == &other;
}
