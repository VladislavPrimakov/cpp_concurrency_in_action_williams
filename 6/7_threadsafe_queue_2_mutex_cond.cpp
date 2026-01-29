#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
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

	std::mutex head_mutex;
	std::unique_ptr<node> head;
	std::mutex tail_mutex;
	node* tail;
	std::condition_variable data_cond;

	node* get_tail() {
		std::lock_guard<std::mutex> tail_lock(tail_mutex);
		return tail;
	}

	std::unique_ptr<node> pop_head() {
		std::unique_ptr<node> old_head = std::move(head);
		head = std::move(old_head->next);
		return old_head;
	}

	std::unique_lock<std::mutex> wait_for_data() {
		std::unique_lock<std::mutex> head_lock(head_mutex);
		data_cond.wait(head_lock, [&] {return head.get() != get_tail(); });
		return head_lock;
	}

	std::unique_ptr<node> wait_pop_head() {
		std::unique_lock<std::mutex> head_lock(wait_for_data());
		return pop_head();
	}

	std::unique_ptr<node> try_pop_head() {
		std::lock_guard<std::mutex> head_lock(head_mutex);
		if (head.get() == get_tail()) {
			return std::unique_ptr<node>();
		}
		return pop_head();
	}

public:
	threadsafe_queue() : head(new node), tail(head.get()) {}
	threadsafe_queue(const threadsafe_queue& other) = delete;
	threadsafe_queue& operator=(const threadsafe_queue& other) = delete;

	std::shared_ptr<T> wait_and_pop() {
		std::unique_ptr<node> const old_head = wait_pop_head();
		return old_head->data;
	}

	std::shared_ptr<T> try_pop() {
		std::unique_ptr<node> old_head = try_pop_head();
		return old_head ? old_head->data : std::shared_ptr<T>();
	}

	void push(T new_value) {
		std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
		std::unique_ptr<node> p(new node);
		{
			std::lock_guard<std::mutex> tail_lock(tail_mutex);
			tail->data = new_data;
			node* const new_tail = p.get();
			tail->next = std::move(p);
			tail = new_tail;
		}
		data_cond.notify_one();
	}

	bool empty() {
		std::lock_guard<std::mutex> head_lock(head_mutex);
		return (head.get() == get_tail());
	}
};

std::atomic<int> processed_count{ 0 };
const int POISON_PILL = -1;

void producer(threadsafe_queue<int>& q, int count) {
	for (int i = 0; i < count; ++i) {
		q.push(i);
	}
}

void consumer(threadsafe_queue<int>& q, int id) {
	while (true) {
		std::shared_ptr<int> data = q.wait_and_pop();
		if (data) {
			int value = *data;
			if (value == POISON_PILL) {
				break;
			}
			processed_count.fetch_add(1, std::memory_order_relaxed);
		}
	}
}

int main() {
	threadsafe_queue<int> queue;

	const int num_producers = 4;
	const int num_consumers = 4;
	const int items_per_producer = 100000;

	std::println("Starting threadsafe_queue test with wait_and_pop (Poison Pill method)...");

	std::vector<std::thread> producers;
	std::vector<std::thread> consumers;

	auto start_time = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < num_consumers; ++i) {
		consumers.emplace_back(consumer, std::ref(queue), i);
	}

	for (int i = 0; i < num_producers; ++i) {
		producers.emplace_back(producer, std::ref(queue), items_per_producer);
	}

	for (auto& t : producers) {
		t.join();
	}

	for (int i = 0; i < num_consumers; ++i) {
		queue.push(POISON_PILL);
	}

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