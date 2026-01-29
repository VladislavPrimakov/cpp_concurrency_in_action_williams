#include <atomic>
#include <print>
#include <thread>
#include <vector>

std::vector<int> queue_data;
std::atomic<int> count;

void process(int item) {
	std::print("Thread {} | Processing item: {}\n", std::this_thread::get_id(), item);
}

void populate_queue() {
	unsigned const number_of_items = 20;
	queue_data.clear();
	for (unsigned i = 0; i < number_of_items; ++i) {
		queue_data.push_back(i);
	}
	count.store(number_of_items, std::memory_order_release);
	count.notify_all();
}

void consume_queue_items() {
	while (true) {
		int current_count = count.load(std::memory_order_acquire);
		while (current_count <= 0) {
			count.wait(current_count, std::memory_order_acquire);
			current_count = count.load(std::memory_order_acquire);
		}
		int item_index = count.fetch_sub(1, std::memory_order_acquire);
		if (item_index <= 0) {
			continue;
		}
		process(queue_data[item_index - 1]);
	}
}

int main() {
	std::jthread a(populate_queue);
	std::jthread b(consume_queue_items);
	std::jthread c(consume_queue_items);
}