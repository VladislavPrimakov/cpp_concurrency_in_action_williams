#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

template<typename T>
class threadsafe_queue {
private:
	mutable std::mutex mut;
	std::queue<std::shared_ptr<T>> data_queue;
	std::condition_variable data_cond;

public:
	threadsafe_queue() {}

	void wait_and_pop(T& value) {
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [this] {return !data_queue.empty(); });
		value = std::move(*data_queue.front());
		data_queue.pop();
	}

	bool try_pop(T& value) {
		std::lock_guard<std::mutex> lk(mut);
		if (data_queue.empty())
			return false;
		value = std::move(*data_queue.front());
		data_queue.pop();
		return true;
	}

	std::shared_ptr<T> wait_and_pop() {
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [this] {return !data_queue.empty(); });
		std::shared_ptr<T> res = std::move(data_queue.front());
		data_queue.pop();
		return res;
	}

	std::shared_ptr<T> try_pop() {
		std::lock_guard<std::mutex> lk(mut);
		if (data_queue.empty())
			return std::shared_ptr<T>();
		std::shared_ptr<T> res = std::move(data_queue.front());
		data_queue.pop();
		return res;
	}

	void push(T new_value) {
		std::shared_ptr<T> data(std::make_shared<T>(std::move(new_value)));
		std::lock_guard<std::mutex> lk(mut);
		data_queue.push(data);
		data_cond.notify_one();
	}

	bool empty() const {
		std::lock_guard<std::mutex> lk(mut);
		return data_queue.empty();
	}
};

std::atomic<int> counter{ 0 };
std::atomic<bool> done{ false };

void producer(threadsafe_queue<int>& q, int items) {
	for (int i = 0; i < items; ++i) {
		q.push(i);
	}
}

void consumer(threadsafe_queue<int>& q) {
	while (true) {
		int val;
		if (!q.try_pop(val)) {
			if (done.load() && q.empty()) {
				break;
			}
			std::this_thread::yield();
		}
		else {
			counter.fetch_add(1);
		}
	}
}

void consumer_ptr(threadsafe_queue<int>& q) {
	while (true) {
		auto ptr = q.try_pop();
		if (!ptr) {
			if (done.load() && q.empty()) {
				break;
			}
			std::this_thread::yield();
		}
		else {
			counter.fetch_add(1);
		}
	}
}

int main() {
	threadsafe_queue<int> queue;
	int items = 10000;
	int producers_count = 4;

	std::vector<std::thread> producers;
	for (int i = 0; i < producers_count; ++i) {
		producers.emplace_back(producer, std::ref(queue), items);
	}

	std::vector<std::thread> consumers;
	for (int i = 0; i < 2; ++i) {
		consumers.emplace_back(consumer, std::ref(queue));
	}
	for (int i = 0; i < 2; ++i) {
		consumers.emplace_back(consumer_ptr, std::ref(queue));
	}

	for (auto& p : producers) {
		p.join();
	}
	done.store(true);

	for (auto& c : consumers) {
		c.join();
	}

	std::cout << "Processed: " << counter.load() << std::endl;
	std::cout << "Expected: " << items * producers_count << std::endl;

	assert(counter.load() == items * producers_count);
}