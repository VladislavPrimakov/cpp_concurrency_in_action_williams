#include <assert.h>
#include <atomic>
#include <thread>

std::atomic<bool> x, y;
std::atomic<int> z;

void write_x_then_y() {
	x.store(true, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_release);
	y.store(true, std::memory_order_relaxed);
	y.notify_all();
}

void read_y_then_x() {
	y.wait(false, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_acquire);
	if (x.load(std::memory_order_relaxed))
		++z;
}

int main() {
	x = false;
	y = false;
	z = 0;
	{
		std::jthread a(write_x_then_y);
		std::jthread b(read_y_then_x);
	}
	assert(z.load() != 0);
}