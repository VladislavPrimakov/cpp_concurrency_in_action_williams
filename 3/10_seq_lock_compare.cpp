#include <chrono>
#include <mutex>
#include <print>

class Y {
private:
	int some_detail;
	mutable std::mutex m;
	int get_detail() const {
		std::lock_guard<std::mutex> lock_a(m);
		return some_detail;
	}
public:
	Y(int sd) :some_detail(sd) {}

	friend bool operator == (Y const& lhs, Y const& rhs) {
		if (&lhs == &rhs)
			return true;
		int const lhs_value = lhs.get_detail();
		int const rhs_value = rhs.get_detail();
		return lhs_value == rhs_value;
	}
};

class Y2 {
private:
	int some_detail;
	mutable std::mutex m;
public:
	Y2(int sd) : some_detail(sd) {}

	friend bool operator == (Y2 const& lhs, Y2 const& rhs) {
		if (&lhs == &rhs)
			return true;
		std::lock_guard<std::mutex> lock_a(lhs.m);
		std::lock_guard<std::mutex> lock_b(rhs.m);
		return lhs.some_detail == rhs.some_detail;
	}
};

int main() {
	int num_iterations = 10000000;
	auto obj1 = Y(1);
	auto obj2 = Y(1);
	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < num_iterations; ++i) {
		bool eq = (obj1 == obj2);
		(void)eq;
	}
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::println("Time taken for Y comparison: {} ms", duration);

	auto obj3 = Y2(1);
	auto obj4 = Y2(1);
	start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < num_iterations; ++i) {
		bool eq = (obj3 == obj4);
		(void)eq;
	}
	end = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::println("Time taken for Y2 comparison: {} ms", duration);
	return 0;
}
