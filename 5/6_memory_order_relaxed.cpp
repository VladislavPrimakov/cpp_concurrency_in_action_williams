#include <atomic>
#include <print>
#include <thread>

std::atomic<int> x(0), y(0), z(0);
std::atomic<bool> go(false);
unsigned const loop_count = 10;

struct read_values {
	int x, y, z;
};

read_values values1[loop_count];
read_values values2[loop_count];
read_values values3[loop_count];
read_values values4[loop_count];
read_values values5[loop_count];

void increment(std::atomic<int>* var_to_inc, read_values* values) {
	while (!go)
		std::this_thread::yield();
	for (unsigned i = 0; i < loop_count; ++i) {
		values[i].x = x.load(std::memory_order_relaxed);
		values[i].y = y.load(std::memory_order_relaxed);
		values[i].z = z.load(std::memory_order_relaxed);
		var_to_inc->store(i + 1, std::memory_order_relaxed);
		std::this_thread::yield();
	}
}

void read_vals(read_values* values) {
	while (!go)
		std::this_thread::yield();
	for (unsigned i = 0; i < loop_count; ++i) {
		values[i].x = x.load(std::memory_order_relaxed);
		values[i].y = y.load(std::memory_order_relaxed);
		values[i].z = z.load(std::memory_order_relaxed);
		std::this_thread::yield();
	}
}

void print(read_values* v) {
	for (unsigned i = 0; i < loop_count; ++i) {
		if (i)
			std::print(",");
		std::print("({},{},{})", v[i].x, v[i].y, v[i].z);
	}
	std::println();
}

int main() {
	{
		std::jthread t1(increment, &x, values1);
		std::jthread t2(increment, &y, values2);
		std::jthread t3(increment, &z, values3);
		std::jthread t4(read_vals, values4);
		std::jthread t5(read_vals, values5);
		go = true;
	}
	print(values1);
	print(values2);
	print(values3);
	print(values4);
	print(values5);
}