#include <assert.h>
#include <atomic>
#include <thread>

std::atomic<bool> x, y;
std::atomic<int> z;

void write_x() {
	x.store(true, std::memory_order_release);
}

void write_y() {
	y.store(true, std::memory_order_release);
}

void read_x_then_y() {
	while (!x.load(std::memory_order_acquire));
	if (y.load(std::memory_order_acquire))
		++z;
}

void read_y_then_x() {
	while (!y.load(std::memory_order_acquire));
	if (x.load(std::memory_order_acquire))
		++z;
}

int main() {
	x = false;
	y = false;
	z = 0;
	{
		std::jthread a(write_x);
		std::jthread b(write_y);
		std::jthread c(read_x_then_y);
		std::jthread d(read_y_then_x);
	}
	assert(z.load() != 0);
}