#include <algorithm>
#include <cassert>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <print>
#include <shared_mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

template<typename Key, typename Value, typename Hash = std::hash<Key>>
class threadsafe_lookup_table {
private:
	class bucket_type {
	private:
		typedef std::pair<Key, Value> bucket_value;
		typedef std::list<bucket_value> bucket_data;
		typedef typename bucket_data::iterator bucket_iterator;
		bucket_data data;
		mutable std::shared_mutex mutex;
		bucket_iterator find_entry_for(Key const& key) {
			return std::find_if(data.begin(), data.end(), [&](bucket_value const& item) { return item.first == key; });
		}
	public:
		Value value_for(Key const& key, Value const& default_value) {
			std::shared_lock<std::shared_mutex> lock(mutex);
			bucket_iterator const found_entry = find_entry_for(key);
			return (found_entry == data.end()) ? default_value : found_entry->second;
		}

		void add_or_update_mapping(Key const& key, Value const& value) {
			std::unique_lock<std::shared_mutex> lock(mutex);
			bucket_iterator const found_entry = find_entry_for(key);
			if (found_entry == data.end()) {
				data.push_back(bucket_value(key, value));
			}
			else {
				found_entry->second = value;
			}
		}

		void remove_mapping(Key const& key) {
			std::unique_lock<std::shared_mutex> lock(mutex);
			bucket_iterator const found_entry = find_entry_for(key);
			if (found_entry != data.end()) {
				data.erase(found_entry);
			}
		}

		std::map<Key, Value> get_map() const {
			std::vector<std::unique_lock<std::shared_mutex> > locks;
			for (unsigned i = 0; i < buckets.size(); ++i) {
				locks.push_back(std::unique_lock<std::shared_mutex>(buckets[i].mutex));
			}
			std::map<Key, Value> res;
			for (unsigned i = 0; i < buckets.size(); ++i) {
				for (bucket_iterator it = buckets[i].data.begin(); it != buckets[i].data.end(); ++it) {
					res.insert(*it);
				}
			}
			return res;
		}
	};

	std::vector<std::unique_ptr<bucket_type>> buckets;
	Hash hasher;

	bucket_type& get_bucket(Key const& key) {
		std::size_t const bucket_index = hasher(key) % buckets.size();
		return *buckets[bucket_index];
	}

public:
	typedef Key key_type;
	typedef Value mapped_type;
	typedef Hash hash_type;

	threadsafe_lookup_table(
		unsigned num_buckets = 19, Hash const& hasher_ = Hash()) : buckets(num_buckets), hasher(hasher_) {
		for (unsigned i = 0; i < num_buckets; ++i) {
			buckets[i].reset(new bucket_type);
		}
	}

	threadsafe_lookup_table(threadsafe_lookup_table const& other) = delete;
	threadsafe_lookup_table& operator=(threadsafe_lookup_table const& other) = delete;

	Value value_for(Key const& key, Value const& default_value = Value()) {
		return get_bucket(key).value_for(key, default_value);
	}

	void add_or_update_mapping(Key const& key, Value const& value) {
		get_bucket(key).add_or_update_mapping(key, value);
	}

	void remove_mapping(Key const& key) {
		get_bucket(key).remove_mapping(key);
	}
};


int main() {
	threadsafe_lookup_table<int, std::string> table;

	std::thread t1([&]() {
		for (int i = 0; i < 100; ++i)
			table.add_or_update_mapping(i, "Value " + std::to_string(i));
		});

	std::thread t2([&]() {
		for (int i = 100; i < 200; ++i)
			table.add_or_update_mapping(i, "Value " + std::to_string(i));
		});

	std::thread t3([&]() {
		for (int i = 0; i < 200; ++i) {
			std::string val = table.value_for(i, "Not Found");
			if (val == "Not Found") {
				// its fine
			}
		}
		});

	t1.join();
	t2.join();
	t3.join();

	assert(table.value_for(50, "default") == "Value 50");
	assert(table.value_for(150, "default") == "Value 150");
	table.remove_mapping(50);
	assert(table.value_for(50, "default") == "default");

	std::println("Test passed!");
	return 0;
}