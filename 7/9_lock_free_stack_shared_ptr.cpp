#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <print>
#include <thread>
#include <vector>

template<typename T>
class lock_free_stack {
private:
	struct node {
		std::shared_ptr<T> data;
		std::atomic<std::shared_ptr<node>> next;

		node(T const& data_) : data(std::make_shared<T>(data_)), next(nullptr) {}
	};

	std::atomic<std::shared_ptr<node>> head;

public:
	void push(T const& data) {
		auto new_node = std::make_shared<node>(data);
		std::shared_ptr<node> old_head = head.load(std::memory_order_relaxed);
		while (true) {
			new_node->next.store(old_head, std::memory_order_relaxed);
			if (head.compare_exchange_weak(old_head, new_node, std::memory_order_release, std::memory_order_relaxed)) {
				break;
			}
		}
	}

	std::shared_ptr<T> pop() {
		auto old_head = head.load(std::memory_order_acquire);
		while (old_head && !head.compare_exchange_weak(old_head, old_head->next.load(std::memory_order_relaxed), std::memory_order_acquire, std::memory_order_relaxed));
		if (old_head) {
			old_head->next.store(nullptr, std::memory_order_release);
			return old_head->data;
		}
		return nullptr;
	}

	~lock_free_stack() {
		while (pop());
	}

	bool is_lock_free() {
		return head.is_lock_free();
	}
};

std::atomic<long long> push_count{ 0 };
std::atomic<long long> pop_count{ 0 };
std::atomic<long long> data_checksum{ 0 };
std::atomic<bool> producers_finished{ false };

void producer(lock_free_stack<int>& stack, int id, int items_to_push) {
	for (int i = 0; i < items_to_push; ++i) {
		int value = i;
		stack.push(value);
		push_count.fetch_add(1, std::memory_order_relaxed);
		data_checksum.fetch_add(value, std::memory_order_relaxed);
	}
}

void consumer(lock_free_stack<int>& stack, int id) {
	while (true) {
		auto ptr = stack.pop();
		if (ptr) {
			pop_count.fetch_add(1, std::memory_order_relaxed);
			data_checksum.fetch_sub(*ptr, std::memory_order_relaxed);
		}
		else {
			if (producers_finished.load(std::memory_order_acquire)) {
				ptr = stack.pop();
				if (ptr) {
					pop_count.fetch_add(1, std::memory_order_relaxed);
					data_checksum.fetch_sub(*ptr, std::memory_order_relaxed);
					continue;
				}
				break;
			}
			std::this_thread::yield();
		}
	}
}

int main() {
	lock_free_stack<int> ts_stack;
	std::println("Is lock-free: {}", ts_stack.is_lock_free());
	const int num_producers = 4;
	const int num_consumers = 4;
	const int items_per_producer = 100000;

	std::vector<std::thread> producers;
	std::vector<std::thread> consumers;

	std::println("Starting lock-free stress test...");

	for (int i = 0; i < num_consumers; ++i) {
		consumers.emplace_back(consumer, std::ref(ts_stack), i);
	}

	for (int i = 0; i < num_producers; ++i) {
		producers.emplace_back(producer, std::ref(ts_stack), i, items_per_producer);
	}

	for (auto& t : producers) {
		t.join();
	}

	producers_finished.store(true, std::memory_order_release);

	for (auto& t : consumers) {
		t.join();
	}

	std::println("Test finished.");
	std::println("Total pushed: {}", push_count.load());
	std::println("Total popped: {}", pop_count.load());
	std::println("Checksum (should be 0): {}", data_checksum.load());


	assert(push_count.load() == pop_count.load());
	assert(data_checksum.load() == 0);
	assert(ts_stack.pop() == nullptr);

	std::println("SUCCESS: Integrity check passed!");

	return 0;
}