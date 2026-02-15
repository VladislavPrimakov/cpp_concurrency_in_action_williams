#include <atomic>
#include <chrono>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <print>
#include <thread>
#include <utility>

std::mutex m;
std::deque<std::packaged_task<void()>> tasks;
std::atomic<bool> shutdown_flag{ false };

bool gui_shutdown_message_received() {
	return shutdown_flag.load();
}

void get_and_process_gui_message() {
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void gui_thread() {
	while (!gui_shutdown_message_received()) {
		get_and_process_gui_message();
		std::packaged_task<void()> task;
		{
			std::lock_guard<std::mutex> lk(m);
			if (tasks.empty()) {
				continue;
			}
			task = std::move(tasks.front());
			tasks.pop_front();
		}
		task();
	}
	std::print("[GUI Thread] Finish");
}

template<typename Func>
std::future<void> post_task_for_gui_thread(Func f) {
	std::packaged_task<void()> task(f);
	std::future<void> res = task.get_future();
	{
		std::lock_guard<std::mutex> lk(m);
		tasks.push_back(std::move(task));
	}
	return res;
}

int main() {
	std::thread gui_bg_thread(gui_thread);
	std::println("[Main] Send task 1 to GUI thread...");
	std::future<void> f1 = post_task_for_gui_thread([]() {
		std::println("Task 1 is running into GUI thread");
		});

	std::println("[Main] Send task 2 to GUI thread...");
	std::future<void> f2 = post_task_for_gui_thread([]() {
		std::println("Task 2 is running into GUI thread");
		});

	f1.wait();
	f2.wait();
	std::println("[Main] Tasks done.");
	shutdown_flag.store(true);
	gui_bg_thread.join();
	return 0;
}