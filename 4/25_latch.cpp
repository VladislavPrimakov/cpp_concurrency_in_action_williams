#include <chrono>
#include <future>
#include <iostream>
#include <latch>
#include <print>
#include <string>
#include <thread>
#include <vector>

struct my_data {
	int id;
	std::string payload;
};

my_data make_data(unsigned i) {
	std::this_thread::sleep_for(std::chrono::milliseconds(100 + i * 50));
	return { static_cast<int>(i), "Data packet " + std::to_string(i) };
}

void do_more_stuff(unsigned i) {
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	std::println("[Thread {}] Finished extra work.", i);
}

void process_data(const std::vector<my_data>& data, unsigned count) {
	for (const auto& item : data) {
		std::println("  ID: {}, Payload: {}", item.id, item.payload);
	}
}

void foo() {
	unsigned const thread_count = 5;
	std::latch done(thread_count);
	std::vector<my_data> data(thread_count);
	std::vector<std::future<void>> threads;
	for (unsigned i = 0; i < thread_count; ++i) {
		threads.push_back(std::async(std::launch::async, [&, i]() {
			data[i] = make_data(i);
			std::println("[Thread {}] Data ready!", i);
			done.count_down();
			do_more_stuff(i);
			}));
	}
	done.wait();
	std::println("[Main] Latch opened! Processing data immediately.");
	process_data(data, thread_count);
	std::println("[Main] Cleaning up threads (waiting for do_more_stuff)...");
}

int main() {
	foo();
	return 0;
}