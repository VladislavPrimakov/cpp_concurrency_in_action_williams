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
	struct node;

	struct counted_node_ptr {
		int external_count;
		node* ptr;
	};

	struct node {
		std::shared_ptr<T> data;
		std::atomic<int> internal_count;
		counted_node_ptr next;
		node(T const& data_) : data(std::make_shared<T>(data_)), internal_count(0) {}
	};

	std::atomic<counted_node_ptr> head;

	void increase_head_count(counted_node_ptr& old_counter) {
		counted_node_ptr new_counter;
		do {
			new_counter = old_counter;
			++new_counter.external_count;
		} while (!head.compare_exchange_strong(old_counter, new_counter, std::memory_order_acquire, std::memory_order_relaxed));
		old_counter.external_count = new_counter.external_count;
	}

public:
	~lock_free_stack() {
		while (pop());
	}

	void push(T const& data) {
		counted_node_ptr new_node;
		new_node.ptr = new node(data);
		new_node.external_count = 1;
		new_node.ptr->next = head.load(std::memory_order_relaxed);
		while (!head.compare_exchange_weak(new_node.ptr->next, new_node, std::memory_order_release, std::memory_order_relaxed));
	}

	std::shared_ptr<T> pop() {
		counted_node_ptr old_head = head.load(std::memory_order_relaxed);
		for (;;) {
			increase_head_count(old_head);
			node* const ptr = old_head.ptr;

			if (!ptr) {
				return std::shared_ptr<T>();
			}

			if (head.compare_exchange_strong(old_head, ptr->next, std::memory_order_relaxed)) {
				std::shared_ptr<T> res;
				res.swap(ptr->data);
				int const count_increase = old_head.external_count - 2;
				if (ptr->internal_count.fetch_add(count_increase, std::memory_order_release) == -count_increase) {
					delete ptr;
				}
				return res;
			}
			else if (ptr->internal_count.fetch_sub(1, std::memory_order_relaxed) == 1) {
				delete ptr;
			}
		}
	}

	bool empty() {
		return head.load().ptr == nullptr;
	}

	bool is_lock_free() {
		return head.is_lock_free();
	}
};


std::atomic<long long> push_count{ 0 };
std::atomic<long long> pop_count{ 0 };
std::atomic<long long> data_checksum{ 0 };
std::atomic<bool> producers_finished{ false };

void producer(lock_free_stack<int>& stack, int items_to_push) {
	for (int i = 0; i < items_to_push; ++i) {
		stack.push(i);
		push_count.fetch_add(1, std::memory_order_relaxed);
		data_checksum.fetch_add(i, std::memory_order_relaxed);
	}
}

void consumer(lock_free_stack<int>& stack) {
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
	lock_free_stack<int> s;
	std::println("Is lock-free implementation? {}", s.is_lock_free());

	lock_free_stack<int> ts_stack;
	const int num_producers = 4;
	const int num_consumers = 4;
	const int items_per_producer = 100000;

	std::vector<std::thread> producers;
	std::vector<std::thread> consumers;

	std::println("Starting split-reference-count stress test...");
	auto start = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < num_consumers; ++i)
		consumers.emplace_back(consumer, std::ref(ts_stack));

	for (int i = 0; i < num_producers; ++i)
		producers.emplace_back(producer, std::ref(ts_stack), items_per_producer);


	for (auto& t : producers)
		t.join();

	producers_finished.store(true, std::memory_order_release);

	for (auto& t : consumers)
		t.join();

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = end - start;

	std::println("Test finished in {:.4f}s", elapsed.count());
	std::println("Total pushed: {}", push_count.load());
	std::println("Total popped: {}", pop_count.load());
	std::println("Checksum: {}", data_checksum.load());

	assert(push_count.load() == pop_count.load());
	assert(data_checksum.load() == 0);
	assert(ts_stack.pop() == nullptr);

	std::println("SUCCESS: Integrity check passed!");
	return 0;
}