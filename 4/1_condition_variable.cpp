#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <queue>
#include <thread>

struct data_chunk {
	int id;
	bool is_last = false;
};

std::mutex mut;
std::queue<data_chunk> data_queue;
std::condition_variable data_cond;

const int MAX_CHUNKS = 10;
int chunks_generated = 0;

bool more_data_to_prepare() {
	return chunks_generated < MAX_CHUNKS;
}

data_chunk prepare_data() {
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	++chunks_generated;
	return data_chunk{ chunks_generated, chunks_generated == MAX_CHUNKS };
}

void process(const data_chunk& data) {
	std::println("Processing data chunk #{}", data.id);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

bool is_last_chunk(const data_chunk& data) {
	return data.is_last;
}

void data_preparation_thread() {
	while (more_data_to_prepare()) {
		data_chunk const data = prepare_data();
		{
			std::lock_guard<std::mutex> lk(mut);
			data_queue.push(data);
		}
		std::println("[Producer] Pushed chunk #{}", data.id);
		data_cond.notify_one();
	}
}

void data_processing_thread() {
	while (true) {
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [] { return !data_queue.empty(); });
		data_chunk data = data_queue.front();
		data_queue.pop();
		lk.unlock();
		process(data);
		if (is_last_chunk(data)) {
			std::println("[Consumer] Last chunk received. Exiting.");
			break;
		}
	}
}

int main() {
	std::thread t1(data_preparation_thread);
	std::thread t2(data_processing_thread);
	t1.join();
	t2.join();
	return 0;
}