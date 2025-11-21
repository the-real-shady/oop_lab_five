#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pmr_dynamic_resource.hpp"
#include "pmr_queue.hpp"

namespace {
struct Complex {
    Complex(int id, std::string value, double score)
        : id(id), value(std::move(value)), score(score) {}

    int id{};
    std::string value;
    double score{};
};
}  // namespace

TEST(PmrQueueTest, MaintainsFifoOrder) {
    DynamicTrackingResource resource("integers");
    PmrQueue<int> queue(&resource);

    EXPECT_TRUE(queue.empty());
    queue.push(10);
    queue.push(20);
    queue.emplace(30);

    ASSERT_EQ(queue.size(), 3);
    EXPECT_EQ(queue.front(), 10);
    EXPECT_EQ(queue.back(), 30);

    queue.pop();
    EXPECT_EQ(queue.front(), 20);
    EXPECT_EQ(queue.back(), 30);

    queue.clear();
    EXPECT_TRUE(queue.empty());
}

TEST(PmrQueueTest, IteratesOverComplexTypes) {
    DynamicTrackingResource resource("complex");
    PmrQueue<Complex> queue(&resource);

    queue.emplace(1, "alpha", 4.2);
    queue.emplace(2, "beta", 1.0);
    queue.emplace(3, "gamma", 9.5);

    std::vector<int> ids;
    std::vector<std::string> labels;
    for (const auto& element : queue) {
        ids.push_back(element.id);
        labels.emplace_back(element.value);
    }

    EXPECT_EQ(ids, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(labels, (std::vector<std::string>{"alpha", "beta", "gamma"}));
}

TEST(PmrQueueTest, CopyAndMoveOperationsPreserveOrder) {
    DynamicTrackingResource resource("copy-move");
    PmrQueue<int> original(&resource);
    original.push(1);
    original.push(2);

    PmrQueue<int> copy(original);
    ASSERT_EQ(copy.size(), 2);
    EXPECT_EQ(copy.front(), 1);
    EXPECT_EQ(copy.back(), 2);

    PmrQueue<int> moved(std::move(original));
    EXPECT_TRUE(original.empty());
    EXPECT_EQ(moved.size(), 2);
    EXPECT_EQ(moved.front(), 1);
    EXPECT_EQ(moved.back(), 2);
}

TEST(PmrQueueTest, PopOnEmptyThrows) {
    DynamicTrackingResource resource("pop-empty");
    PmrQueue<int> queue(&resource);
    EXPECT_THROW(queue.pop(), std::runtime_error);
}

TEST(PmrQueueTest, FrontAndBackThrowOnEmpty) {
    DynamicTrackingResource resource("front-back-empty");
    PmrQueue<int> queue(&resource);
    EXPECT_THROW(queue.front(), std::runtime_error);
    EXPECT_THROW(queue.back(), std::runtime_error);
}

TEST(PmrQueueTest, HandlesSingleElementLifecycle) {
    DynamicTrackingResource resource("single-element");
    PmrQueue<long long> queue(&resource);
    queue.push(std::numeric_limits<long long>::min());
    ASSERT_EQ(queue.size(), 1);
    EXPECT_EQ(queue.front(), queue.back());
    EXPECT_EQ(queue.front(), std::numeric_limits<long long>::min());
    queue.pop();
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(PmrQueueTest, HandlesManyElements) {
    DynamicTrackingResource resource("bulk");
    PmrQueue<int> queue(&resource);

    constexpr int kCount = 512;
    for (int i = 0; i < kCount; ++i) {
        queue.emplace(i);
    }
    ASSERT_EQ(queue.size(), static_cast<std::size_t>(kCount));
    for (int i = 0; i < kCount; ++i) {
        EXPECT_EQ(queue.front(), i);
        queue.pop();
    }
    EXPECT_TRUE(queue.empty());
}

TEST(DynamicTrackingResourceTest, ReusesReleasedBlocks) {
    DynamicTrackingResource resource("recycle");
    PmrQueue<int> queue(&resource);

    int first_value = queue.emplace(42);
    (void)first_value;
    ASSERT_EQ(resource.tracked_blocks(), 1);
    ASSERT_EQ(resource.active_blocks(), 1);

    queue.pop();
    EXPECT_EQ(resource.active_blocks(), 0);

    queue.emplace(7);
    EXPECT_EQ(resource.tracked_blocks(), 1);  // ранее выделенный блок используется повторно
    EXPECT_EQ(resource.active_blocks(), 1);

    queue.clear();
    EXPECT_EQ(resource.active_blocks(), 0);
    EXPECT_EQ(resource.tracked_blocks(), 1);
}
