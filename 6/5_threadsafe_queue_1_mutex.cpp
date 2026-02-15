#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <print>
#include <thread>
#include <vector>

template<typename T>
class threadsafe_queue {
private:
	struct node {
		std::shared_ptr<T> data;
		std::unique_ptr<node> next;
	};

	mutable std::mutex mut;
	std::unique_ptr<node> head;
	node* tail;
public:
	threadsafe_queue() : head(new node), tail(head.get()) {}
	threadsafe_queue(const threadsafe_queue&) = delete;
	threadsafe_queue& operator=(const threadsafe_queue&) = delete;

	std::shared_ptr<T> try_pop() {
		std::lock_guard<std::mutex> lk(mut);
		if (head.get() == tail) {
			return std::shared_ptr<T>();
		}
		std::shared_ptr<T> res = head->data;
		std::unique_ptr<node> old_head = std::move(head);
		head = std::move(old_head->next);
		return res;
	}

	void push(T new_value) {
		std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
		std::unique_ptr<node> p(new node);
		node* const new_tail = p.get();
		std::lock_guard<std::mutex> lk(mut);
		tail->data = new_data;
		tail->next = std::move(p);
		tail = new_tail;
	}
};

std::atomic<int> processed_count{ 0 };
std::atomic<bool> producers_finished{ false };

void producer(threadsafe_queue<int>& q, int items_count) {
	for (int i = 0; i < items_count; ++i) {
		q.push(i);
	}
}

void consumer(threadsafe_queue<int>& q) {
	while (true) {
		auto data = q.try_pop();
		if (data) {
			processed_count.fetch_add(1, std::memory_order_relaxed);
		}
		else {
			if (producers_finished.load(std::memory_order_acquire)) {
				// double check if queue is really empty
				if (!q.try_pop()) {
					break;
				}
				else {
					processed_count.fetch_add(1, std::memory_order_relaxed);
				}
			}
			else {
				// productors are still running, yield to avoid busy waiting
				std::this_thread::yield();
			}
		}
	}
}

int main() {
	threadsafe_queue<int> queue;

	const int num_producers = 4;
	const int num_consumers = 4;
	const int items_per_producer = 100000;

	std::println("Starting threadsafe_queue test...");

	std::vector<std::thread> producers;
	std::vector<std::thread> consumers;

	auto start_time = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < num_consumers; ++i) {
		consumers.emplace_back(consumer, std::ref(queue));
	}

	for (int i = 0; i < num_producers; ++i) {
		producers.emplace_back(producer, std::ref(queue), items_per_producer);
	}

	for (auto& t : producers) {
		t.join();
	}

	producers_finished.store(true, std::memory_order_release);

	for (auto& t : consumers) {
		t.join();
	}

	auto end_time = std::chrono::high_resolution_clock::now();
	std::println("Estimated: {}", std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time));

	int expected_items = num_producers * items_per_producer;
	int actual_items = processed_count.load();

	std::println("Expected items: {}", expected_items);
	std::println("Processed items: {}", actual_items);

	assert(expected_items == actual_items);
	return 0;
}