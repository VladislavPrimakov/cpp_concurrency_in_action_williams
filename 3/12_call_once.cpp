#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class Connection {
public:
	void send() {
		volatile int x = 0;
		(void)x;
	}
};

class ModernX {
private:
	Connection* connection = nullptr;
	std::once_flag init_flag;

	void open_connection() {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		connection = new Connection();
	}
public:
	~ModernX() { delete connection; }

	void send_data() {
		std::call_once(init_flag, &ModernX::open_connection, this);
		connection->send();
	}
};

class OldX {
private:
	Connection* connection = nullptr;
	std::mutex mtx;
	std::atomic<bool> is_initialized{ false };

	void open_connection() {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		connection = new Connection();
		is_initialized.store(true, std::memory_order_release);
	}
public:
	~OldX() { delete connection; }

	void send_data() {
		if (!is_initialized.load(std::memory_order_acquire)) {
			std::lock_guard<std::mutex> lock(mtx);
			if (!is_initialized.load(std::memory_order_relaxed)) {
				open_connection();
			}
		}
		connection->send();
	}
};

class TwoFlagsX {
	Connection* connection = nullptr;
	std::atomic_flag init_starting = ATOMIC_FLAG_INIT;
	std::atomic<bool> init_completed{ false };

	void open_connection() {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		connection = new Connection();
	}
public:
	~TwoFlagsX() { delete connection; }

	void send_data() {
		if (init_completed.load(std::memory_order_acquire)) {
			connection->send();
			return;
		}
		if (!init_starting.test_and_set(std::memory_order_acquire)) {
			open_connection();
			init_completed.store(true, std::memory_order_release);
		}
		else {
			while (!init_completed.load(std::memory_order_acquire)) {
				std::this_thread::yield();
			}
		}
		connection->send();
	}
};

template <typename T>
void run_benchmark(const std::string& name) {
	T obj;
	const int num_threads = 4;
	const int num_iterations = 10000000;
	std::vector<std::thread> threads;
	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < num_threads; ++i) {
		threads.emplace_back([&obj, num_iterations]() {
			for (int j = 0; j < num_iterations; ++j) {
				obj.send_data();
			}
			});
	}
	for (auto& t : threads)
		t.join();
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> duration = end - start;
	std::cout << name << ": " << duration.count() << " ms" << std::endl;
}

int main() {
	run_benchmark<ModernX>("Modern (std::call_once)");
	run_benchmark<OldX>("Old (Double-Checked Locking)");
	run_benchmark<TwoFlagsX>("TwoFlagsX");

	return 0;
}