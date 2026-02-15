#include <atomic>
#include <cassert>
#include <exception>
#include <memory>
#include <mutex>
#include <print>
#include <stack>
#include <thread>
#include <vector>

struct empty_stack : std::exception {
	const char* what() const noexcept override {
		return "empty stack";
	}
};

template<typename T>
class threadsafe_stack {
private:
	std::stack<T> data;
	mutable std::mutex m;
public:
	threadsafe_stack() {}

	threadsafe_stack(const threadsafe_stack& other) {
		std::lock_guard<std::mutex> lock(other.m);
		data = other.data;
	}

	threadsafe_stack& operator=(const threadsafe_stack&) = delete;

	void push(T new_value) {
		std::lock_guard<std::mutex> lock(m);
		data.push(std::move(new_value));
	}

	std::shared_ptr<T> pop() {
		std::lock_guard<std::mutex> lock(m);
		if (data.empty()) throw empty_stack();
		std::shared_ptr<T> res(std::make_shared<T>(std::move(data.top())));
		data.pop();
		return res;
	}

	void pop(T& value) {
		std::lock_guard<std::mutex> lock(m);
		if (data.empty()) throw empty_stack();
		value = std::move(data.top());
		data.pop();
	}

	bool empty() const {
		std::lock_guard<std::mutex> lock(m);
		return data.empty();
	}
};


std::atomic<int> push_count{ 0 };
std::atomic<int> pop_count{ 0 };
std::atomic<bool> producers_finished{ false };

void producer(threadsafe_stack<int>& stack, int id, int items_to_push) {
	for (int i = 0; i < items_to_push; ++i) {
		stack.push(i);
		push_count.fetch_add(1, std::memory_order_relaxed);
	}
	std::println("Producer {} finished", id);
}

void consumer(threadsafe_stack<int>& stack, int id) {
	while (true) {
		try {
			int value;
			stack.pop(value);
			pop_count.fetch_add(1, std::memory_order_relaxed);
		}
		catch (const empty_stack&) {
			if (producers_finished.load(std::memory_order_acquire)) {
				// if no producers check empry and break
				if (stack.empty()) {
					break;
				}
			}
			else {
				// wait if producers still work
				std::this_thread::yield();
			}
		}
	}
	std::println("Consumer {} finished", id);
}

int main() {
	threadsafe_stack<int> ts_stack;
	const int num_producers = 4;
	const int num_consumers = 4;
	const int items_per_producer = 10000;

	std::vector<std::thread> producers;
	std::vector<std::thread> consumers;

	std::println("Starting stress test...");

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

	assert(push_count == pop_count);
	assert(ts_stack.empty());

	return 0;
}