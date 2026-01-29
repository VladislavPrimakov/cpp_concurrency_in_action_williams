#include <assert.h>
#include <atomic>
#include <thread>

bool x;
std::atomic<bool> y;
std::atomic<int> z;

void write_x_then_y() {
	x = true;
	std::atomic_thread_fence(std::memory_order_release);
	y.store(true, std::memory_order_relaxed);
	y.notify_all();
}

void read_y_then_x() {
	y.wait(false, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_acquire);
	if (x)
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