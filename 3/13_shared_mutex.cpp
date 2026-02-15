#include <map> 
#include <mutex>
#include <print>
#include <shared_mutex> 
#include <string> 
#include <thread>

class dns_entry {};

class dns_cache {
	std::map<std::string, dns_entry> entries;
	mutable std::shared_mutex entry_mutex;
public:
	std::optional<dns_entry> find_entry(std::string const& domain) const {
		std::shared_lock<std::shared_mutex> lk(entry_mutex);
		auto const it = entries.find(domain);
		return (it == entries.end()) ? std::nullopt : std::optional<dns_entry>(it->second);
	}

	void update_or_add_entry(std::string const& domain, dns_entry const& dns_details) {
		std::lock_guard<std::shared_mutex> lk(entry_mutex);
		entries[domain] = dns_details;
	}
};

int main() {
	dns_cache cache;
	std::thread th1 = std::thread([&cache]() {
		std::println("Thread 1: Looking up example.com");
		auto entry = cache.find_entry("example.com");
		if (entry)
			std::println("Thread 1: Found entry for example.com");
		else
			std::println("Thread 1: No entry for example.com");
		});
	std::thread th2 = std::thread([&cache]() {
		std::println("Thread 2: Updating example.com");
		cache.update_or_add_entry("example.com", dns_entry{});
		});
	th2.join();
	th1.join();
	return 0;
}
