#include "pmr_dynamic_resource.hpp"
#include "pmr_queue.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

struct Task {
    int id{};
    std::string summary;
    double hours{};
    bool completed{};

    Task(int identifier, std::string_view short_summary, double estimated_hours, bool is_completed = false)
        : id(identifier),
          summary(short_summary),
          hours(estimated_hours),
          completed(is_completed) {}
};

std::ostream& operator<<(std::ostream& stream, const Task& task) {
    stream << "[#" << task.id << "] " << task.summary << " (" << std::fixed << std::setprecision(1)
           << task.hours << "h, done=" << std::boolalpha << task.completed << ")";
    return stream;
}

void print_resource_state(const DynamicTrackingResource& resource) {
    std::cout << "\nMemory resource '" << resource.label() << "':\n";
    std::cout << "  tracked blocks : " << resource.tracked_blocks() << '\n';
    std::cout << "  active blocks  : " << resource.active_blocks() << '\n';
    std::cout << "  bytes in use   : " << resource.bytes_in_use() << '\n';
    std::cout << "  block details  :\n";

    int index = 0;
    for (const auto& block : resource.snapshot()) {
        std::cout << "    #" << index++
                  << " size=" << block.size
                  << " alignment=" << block.alignment
                  << " state=" << (block.in_use ? "in use" : "free") << '\n';
    }
}

int main() {
    DynamicTrackingResource resource("queue-lab");

    PmrQueue<int> numbers(&resource);
    numbers.push(10);
    numbers.push(20);
    numbers.emplace(30);
    numbers.emplace(40);

    std::cout << "Integer queue contents: ";
    for (int value : numbers) {
        std::cout << value << ' ';
    }
    std::cout << "\nFront=" << numbers.front() << ", Back=" << numbers.back() << '\n';

    numbers.pop();
    numbers.emplace(50);
    std::cout << "After pop + emplace: ";
    for (int value : numbers) {
        std::cout << value << ' ';
    }
    std::cout << '\n';

    PmrQueue<Task> tasks(&resource);
    tasks.emplace(1, "Design allocator", 5.5);
    tasks.emplace(2, "Wire queue iterator", 3.5);
    tasks.emplace(3, "Document demo", 2.0, true);

    std::cout << "\nTask queue:\n";
    for (const Task& task : tasks) {
        std::cout << "  " << task << '\n';
    }

    print_resource_state(resource);

    numbers.clear();
    tasks.pop();
    tasks.emplace(4, "Review solution", 1.2);

    std::cout << "\nQueues after reuse:\n";
    std::cout << "  Integers empty? " << std::boolalpha << numbers.empty() << '\n';
    std::cout << "  Tasks count: " << tasks.size() << '\n';

    print_resource_state(resource);

    tasks.clear();
    std::cout << "\nAll queues cleared.\n";
    print_resource_state(resource);

    return 0;
}
