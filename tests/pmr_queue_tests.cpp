#include <gtest/gtest.h>

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
