#include <future>
#include <map>
#include <memory>
#include <print>
#include <queue>
#include <string>
#include <vector>

using payload_type = std::string;

struct data_packet {
	int id;
	payload_type payload;
};

struct outgoing_packet {
	payload_type payload;
	std::shared_ptr<std::promise<bool>> promise;
};

class Connection {
public:
	int id;
	std::queue<data_packet> incoming_queue;
	std::queue<outgoing_packet> outgoing_queue;
	std::map<int, std::shared_ptr<std::promise<payload_type>>> promise_map;

	Connection(int id) : id(id) {}

	bool has_incoming_data() const {
		return !incoming_queue.empty();
	}

	data_packet incoming() {
		data_packet p = incoming_queue.front();
		incoming_queue.pop();
		std::println("[Conn {}] Received packet ID: {}", id, p.id);
		return p;
	}

	std::promise<payload_type>& get_promise(int data_id) {
		return *promise_map.at(data_id);
	}

	bool has_outgoing_data() const {
		return !outgoing_queue.empty();
	}

	outgoing_packet top_of_outgoing_queue() {
		outgoing_packet p = outgoing_queue.front();
		outgoing_queue.pop();
		return p;
	}

	void send(const payload_type& payload) {
		std::println("[Conn {}] Sending data to network: '{}'", id, payload);
	}
};

bool done(std::vector<Connection>& connections) {
	for (const auto& c : connections) {
		if (c.has_incoming_data() || c.has_outgoing_data()) {
			return false;
		}
	}
	return true;
}

void process_connections(std::vector<Connection>& connections) {
	while (!done(connections)) {
		for (auto connection = connections.begin(); connection != connections.end(); ++connection) {
			if (connection->has_incoming_data()) {
				data_packet data = connection->incoming();
				std::promise<payload_type>& p = connection->get_promise(data.id);
				p.set_value(data.payload);
			}
			if (connection->has_outgoing_data()) {
				outgoing_packet data = connection->top_of_outgoing_queue();
				connection->send(data.payload);
				data.promise->set_value(true);
			}
		}
	}
}

int main() {
	std::vector<Connection> connections;
	connections.emplace_back(1);
	Connection& conn = connections[0];

	// --- Scenario 1: Waiting for incoming data ---
	// We expect a response with ID 100. We create a promise/future pair.
	auto response_promise = std::make_shared<std::promise<std::string>>();
	auto response_future = response_promise->get_future();
	conn.promise_map.insert({ 100, response_promise });
	std::println("Simulating incoming network data...");
	conn.incoming_queue.push({ 100, "Server Response Data" });


	// --- Scenario 2: Sending outgoing data ---
	// We want to send data and know when it's sent.
	auto send_confirmation_promise = std::make_shared<std::promise<bool>>();
	auto send_future = send_confirmation_promise->get_future();
	std::println("Queueing outgoing data...");
	conn.outgoing_queue.push({ "Client Request Data", send_confirmation_promise });


	std::println("\n--- Starting Event Loop ---\n");
	process_connections(connections);
	std::println("\n--- Event Loop Finished ---\n");

	// 1. Check incoming result
	if (response_future.valid()) {
		std::println("Result from ID 100: {}", response_future.get());
	}

	// 2. Check outgoing confirmation
	if (send_future.valid()) {
		if (send_future.get()) {
			std::println("Outgoing data sent successfully!");
		}
	}
	return 0;
}