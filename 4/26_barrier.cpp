#include <algorithm>
#include <barrier>
#include <iostream>
#include <mutex>
#include <numeric>
#include <print>
#include <thread>
#include <vector>

struct DataChunk {
	int value;
};

struct ResultChunk {
	int processed_value;
};

using ResultBlock = std::vector<ResultChunk>;

class DataSource {
	std::mutex m;
	int current_val = 0;
	const int max_val = 20;
public:
	bool done() {
		return current_val >= max_val;
	}

	std::vector<int> get_next_data_block() {
		std::vector<int> block;
		int num = 5;
		for (int i = 0; i < num && current_val < max_val; ++i) {
			block.push_back(++current_val);
		}
		return block;
	}
};

class DataSink {
public:
	void write_data(ResultBlock result) {
		std::print("Sink received block: [ ");
		for (const auto& r : result) {
			std::print("{} ", r.processed_value);
		}
		std::println("]");
	}
};

ResultChunk process(DataChunk chunk) {
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	return ResultChunk{ chunk.value * chunk.value };
}

std::vector<DataChunk> divide_into_chunks(const std::vector<int>& data, unsigned num_threads) {
	std::vector<DataChunk> chunks;
	for (unsigned i = 0; i < num_threads; ++i) {
		if (i < data.size()) {
			chunks.push_back({ data[i] });
		}
	}
	return chunks;
}

void process_data(DataSource& source, DataSink& sink) {
	unsigned const concurrency = std::thread::hardware_concurrency();
	unsigned const num_threads = (concurrency > 0) ? concurrency : 2;
	std::println("Starting processing with {} threads...", num_threads);
	std::vector<DataChunk> chunks;
	ResultBlock result;
	std::vector<std::jthread> threads;
	threads.reserve(num_threads);
	bool no_more_data = false;
	auto split_action = [&]() {
		if (source.done()) {
			no_more_data = true;
			chunks.clear();
		}
		else {
			auto current_block = source.get_next_data_block();
			chunks = divide_into_chunks(current_block, num_threads);
			result.resize(chunks.size());
		}
		};
	//split_action();
	std::barrier sync1(num_threads, split_action);
	auto sink_action = [&]() {
		std::println("[Sink] Writing processed data...");
		if (!chunks.empty()) {
			sink.write_data(std::move(result));
		}
		};
	std::barrier sync2(num_threads, sink_action);
	for (unsigned i = 0; i < num_threads; ++i) {
		threads.emplace_back([&, i]() {
			while (!no_more_data) {
				std::println("[Thread {}] Waiting at sync1...", i);
				sync1.arrive_and_wait();
				if (i < chunks.size()) {
					result[i] = process(chunks[i]);
				}
				sync2.arrive_and_wait();
				std::println("[Thread {}] Waiting at sync2...", i);
			}
			});
	}
}

int main() {
	DataSource source;
	DataSink sink;
	process_data(source, sink);
	return 0;
}