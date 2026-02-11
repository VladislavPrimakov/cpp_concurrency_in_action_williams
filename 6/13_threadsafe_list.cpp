#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <print>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

template<typename T>
class threadsafe_list {
	struct node {
		std::mutex m;
		std::shared_ptr<T> data;
		std::unique_ptr<node> next;
		node() : next() {}
		node(T const& value) : data(std::make_shared<T>(value)) {}
	};
	node head;
public:
	threadsafe_list() {}

	~threadsafe_list() {
		remove_if([](node const&) {return true; });
	}

	threadsafe_list(threadsafe_list const& other) = delete;

	threadsafe_list& operator=(threadsafe_list const& other) = delete;

	void push_front(T const& value) {
		std::unique_ptr<node> new_node(new node(value));
		std::lock_guard<std::mutex> lk(head.m);
		new_node->next = std::move(head.next);
		head.next = std::move(new_node);
	}

	template<typename Function> requires std::invocable<Function, T&>
	void for_each(Function f) {
		node* current = &head;
		std::unique_lock<std::mutex> lk(head.m);
		while (node* const next = current->next.get())
		{
			std::unique_lock<std::mutex> next_lk(next->m);
			lk.unlock();
			f(*next->data);
			current = next;
			lk = std::move(next_lk);
		}
	}

	template<typename Predicate> requires std::predicate<Predicate, T const&>
	std::shared_ptr<T> find_first_if(Predicate p) {
		node* current = &head;
		std::unique_lock<std::mutex> lk(head.m);
		while (node* const next = current->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			lk.unlock();
			if (p(*next->data)) {
				return next->data;
			}
			current = next;
			lk = std::move(next_lk);
		}
		return std::shared_ptr<T>();
	}

	template<typename Predicate> requires std::predicate<Predicate, T const&>
	void remove_if(Predicate p) {
		node* current = &head;
		std::unique_lock<std::mutex> lk(head.m);
		while (node* const next = current->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			if (p(*next->data)) {
				std::unique_ptr<node> old_next = std::move(current->next);
				current->next = std::move(next->next);
				next_lk.unlock();
			}
			else {
				lk.unlock();
				current = next;
				lk = std::move(next_lk);
			}
		}
	}
};

void data_producer(threadsafe_list<int>& list) {
	for (int i = 0; i < 100; ++i) {
		list.push_front(i);
	}
}

void data_cleaner(threadsafe_list<int>& list) {
	// remove all even numbers
	list.remove_if([](int const& val) {
		return val % 2 == 0;
		});
}

void data_printer(threadsafe_list<int>& list) {
	int count = 0;
	list.for_each([&count](int& val) {
		count++;
		});
	std::println("Counted items: {}", count);
}

int main() {
	threadsafe_list<int> list;

	{
		std::jthread t1(data_producer, std::ref(list));
		std::jthread t2(data_producer, std::ref(list));

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		std::jthread t3(data_cleaner, std::ref(list));
	}

	int count = 0;
	list.for_each([&count](int& val) {
		count++;
		});

	assert(count <= 100);
	std::println("Test passed!");

	return 0;
}