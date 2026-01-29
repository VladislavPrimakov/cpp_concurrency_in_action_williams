#include <atomic>
#include <chrono>
#include <print>
#include <thread>
#include <vector>

std::vector<int> data;
std::atomic<bool> data_ready(false);

void reader_thread() {
	while (!data_ready.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	std::println("The answer={}", data[0]);
}

void writer_thread() {
	data.push_back(42);
	data_ready = true;
}

int main() {
	std::thread t1(reader_thread);
	std::thread t2(writer_thread);
	t1.join();
	t2.join();
	return 0;
}