#include <condition_variable>
#include <mutex>
#include <print>
#include <queue>
#include <string>
#include <thread>
#include <variant>

namespace messaging { class sender; }

struct close_queue {};

// ==========================================
// Financial transactions and bank responses
// ==========================================
namespace bank {
	// [ATM] -> [Bank]
	// Request to lock funds and prepare for withdrawal.
	// Contains a callback queue (atm_queue) for the response.
	struct withdraw {
		std::string account;
		unsigned amount;
		mutable messaging::sender* atm_queue;
		withdraw(std::string const& a, unsigned amt, messaging::sender* q) : account(a), amount(amt), atm_queue(q) {}
	};

	// [ATM] -> [Bank]
	// Request to verify if the PIN matches the account.
	struct verify_pin {
		std::string account;
		std::string pin;
		mutable messaging::sender* atm_queue;
		verify_pin(std::string const& a, std::string const& p, messaging::sender* q) : account(a), pin(p), atm_queue(q) {}
	};

	// [ATM] -> [Bank]
	// Request to retrieve current account balance.
	struct get_balance {
		std::string account;
		mutable messaging::sender* atm_queue;
		get_balance(std::string const& a, messaging::sender* q) : account(a), atm_queue(q) {}
	};

	// [ATM] -> [Bank]
	// Request to cancel a previously started withdrawal transaction (unlock funds).
	struct cancel_withdrawal {
		std::string account;
		unsigned amount;
		cancel_withdrawal(std::string const& a, unsigned amt) : account(a), amount(amt) {}
	};

	// [ATM] -> [Bank]
	// Confirmation: Money has been physically dispensed.
	// Bank should finalize the deduction.
	struct withdrawal_processed {
		std::string account;
		unsigned amount;
		withdrawal_processed(std::string const& a, unsigned amt) : account(a), amount(amt) {}
	};

	// [Bank] -> [ATM]
	// Reply: The PIN was correct.
	struct pin_verified {};

	// [Bank] -> [ATM]
	// Reply: The PIN was wrong.
	struct pin_incorrect {};

	// [Bank] -> [ATM]
	// Reply: Funds are available and locked. Proceed to dispense.
	struct withdrawal_approved {};

	// [Bank] -> [ATM]
	// Reply: Not enough money on the account.
	struct withdrawal_denied {};

	// [Bank] -> [ATM]
	// Reply: Contains the requested balance amount.
	struct balance_received {
		unsigned amount;
		explicit balance_received(unsigned amt) : amount(amt) {}
	};
}

// ==========================================
// Screen, keypad, card reader, dispenser
// ==========================================
namespace interface {
	// [ATM] -> [Hardware Interface]
	// Command to mechanically dispense cash to the user.
	struct issue_money {
		unsigned amount;
		explicit issue_money(unsigned amt) : amount(amt) {}
	};

	// [ATM] -> [Hardware Interface]
	// Command to return the physical card to the user.
	struct eject_card {};

	// [ATM] -> [Hardware Interface]
	// Tell UI to show "Enter PIN" screen.
	struct display_enter_pin {};

	// [ATM] -> [Hardware Interface]
	// Tell UI to show "Insert Card" screen.
	struct display_enter_card {};

	// [ATM] -> [Hardware Interface]
	// Tell UI to show error about lack of money.
	struct display_insufficient_funds {};

	// [ATM] -> [Hardware Interface]
	// Tell UI to show "Transaction Cancelled" message.
	struct display_withdrawal_cancelled {};

	// [ATM] -> [Hardware Interface]
	// Tell UI to show "Wrong PIN" error.
	struct display_pin_incorrect_message {};

	// [ATM] -> [Hardware Interface]
	// Tell UI to show the menu (Withdraw/Balance/Cancel).
	struct display_withdrawal_options {};

	// [ATM] -> [Hardware Interface]
	// Tell UI to show the numeric balance.
	struct display_balance {
		unsigned amount;
		explicit display_balance(unsigned amt) : amount(amt) {}
	};

	// [Card Reader] -> [ATM]
	// User inserted a card.
	struct card_inserted {
		std::string account;
		explicit card_inserted(std::string const& a) : account(a) {}
	};

	// [Keypad] -> [ATM]
	// User pressed a numeric key (0-9).
	struct digit_pressed {
		char digit;
		explicit digit_pressed(char d) : digit(d) {}
	};

	// [Keypad] -> [ATM]
	// User pressed the "Clear" / "Backspace" button.
	struct clear_last_pressed {};

	// [Keypad] -> [ATM]
	// User pressed the "Cancel" button.
	struct cancel_pressed {};

	// [Keypad] -> [ATM]
	// User selected "Withdraw" option from the menu.
	struct withdraw_pressed {
		unsigned amount;
		explicit withdraw_pressed(unsigned amt) : amount(amt) {}
	};

	// [Keypad] -> [ATM]
	// User selected "Check Balance" option.
	struct balance_pressed {};
}

using Message = std::variant<
	// System
	close_queue,
	// Bank Commands
	bank::withdraw, bank::cancel_withdrawal, bank::verify_pin, bank::get_balance, bank::withdrawal_processed,
	// Bank Events
	bank::withdrawal_approved, bank::withdrawal_denied, bank::pin_verified, bank::pin_incorrect, bank::balance_received,
	// Interface Commands
	interface::issue_money, interface::eject_card, interface::display_enter_pin, interface::display_enter_card,
	interface::display_insufficient_funds, interface::display_withdrawal_cancelled, interface::display_pin_incorrect_message,
	interface::display_withdrawal_options, interface::display_balance,
	// Interface Events
	interface::card_inserted, interface::digit_pressed, interface::clear_last_pressed, interface::cancel_pressed,
	interface::withdraw_pressed, interface::balance_pressed
>;

// Helper for visit (Overload pattern)
template<class... Ts>
struct overloaded : Ts... {
	using Ts::operator()...;
};

namespace messaging {
	class queue {
		std::mutex m;
		std::condition_variable c;
		std::queue<Message> q;
	public:
		void push(Message const& msg) {
			std::lock_guard<std::mutex> lk(m);
			q.push(msg);
			c.notify_all();
		}
		Message wait_and_pop() {
			std::unique_lock<std::mutex> lk(m);
			c.wait(lk, [&] { return !q.empty(); });
			auto res = q.front();
			q.pop();
			return res;
		}
	};

	class sender {
		queue* q;
	public:
		sender() : q(nullptr) {}
		explicit sender(queue* q_) : q(q_) {}
		void send(Message const& msg) {
			if (q) q->push(msg);
		}
	};

	class receiver {
		queue q;
	public:
		sender get_sender() { return sender(&q); }
		Message wait() { return q.wait_and_pop(); }
	};
}

class atm {
	messaging::receiver inbox;
	messaging::sender to_bank;
	messaging::sender to_interface;

	// Pointer to member function (current state)
	void (atm::* state)();

	std::string account;
	unsigned withdrawal_amount;
	std::string pin;

	// Message filtering (Selective Receive)
	template<typename... Handlers>
	void process_messages(Handlers&&... handlers) {
		bool handled = false;
		while (!handled) {
			auto msg = inbox.wait();
			handled = std::visit(overloaded{
				[&](close_queue const&) { throw close_queue(); return true; }, // Always handle exit
				[&](auto const&) { return false; }, // Ignore unexpected messages by default
				std::forward<Handlers>(handlers)... // User-provided handlers
				}, msg);
		}
	}

	void process_withdrawal() {
		process_messages(
			[&](bank::withdrawal_approved const&) {
				to_interface.send(interface::issue_money(withdrawal_amount));
				to_bank.send(bank::withdrawal_processed(account, withdrawal_amount));
				state = &atm::done_processing;
				return true;
			},
			[&](bank::withdrawal_denied const&) {
				to_interface.send(interface::display_insufficient_funds());
				state = &atm::done_processing;
				return true;
			},
			[&](interface::cancel_pressed const&) {
				to_bank.send(bank::cancel_withdrawal(account, withdrawal_amount));
				to_interface.send(interface::display_withdrawal_cancelled());
				state = &atm::done_processing;
				return true;
			}
		);
	}

	void process_balance() {
		process_messages(
			[&](bank::balance_received const& msg) {
				to_interface.send(interface::display_balance(msg.amount));
				state = &atm::wait_for_action;
				return true;
			},
			[&](interface::cancel_pressed const&) {
				state = &atm::done_processing;
				return true;
			}
		);
	}

	void wait_for_action() {
		to_interface.send(interface::display_withdrawal_options());
		process_messages(
			[&](interface::withdraw_pressed const& msg) {
				withdrawal_amount = msg.amount;
				static messaging::sender my_sender = inbox.get_sender();
				to_bank.send(bank::withdraw(account, msg.amount, &my_sender));
				state = &atm::process_withdrawal;
				return true;
			},
			[&](interface::balance_pressed const&) {
				static messaging::sender my_sender = inbox.get_sender();
				to_bank.send(bank::get_balance(account, &my_sender));
				state = &atm::process_balance;
				return true;
			},
			[&](interface::cancel_pressed const&) {
				state = &atm::done_processing;
				return true;
			}
		);
	}

	void verifying_pin() {
		process_messages(
			[&](bank::pin_verified const&) {
				state = &atm::wait_for_action;
				return true;
			},
			[&](bank::pin_incorrect const&) {
				to_interface.send(interface::display_pin_incorrect_message());
				state = &atm::done_processing;
				return true;
			},
			[&](interface::cancel_pressed const&) {
				state = &atm::done_processing;
				return true;
			}
		);
	}

	void getting_pin() {
		process_messages(
			[&](interface::digit_pressed const& msg) {
				pin += msg.digit;
				if (pin.length() == 4) {
					static messaging::sender my_sender = inbox.get_sender();
					to_bank.send(bank::verify_pin(account, pin, &my_sender));
					state = &atm::verifying_pin;
					return true;
				}
				return false;
			},
			[&](interface::clear_last_pressed const&) {
				if (!pin.empty()) pin.pop_back();
				return false;
			},
			[&](interface::cancel_pressed const&) {
				state = &atm::done_processing;
				return true;
			}
		);
	}

	void waiting_for_card() {
		to_interface.send(interface::display_enter_card());
		process_messages(
			[&](interface::card_inserted const& msg) {
				account = msg.account;
				pin = "";
				to_interface.send(interface::display_enter_pin());
				state = &atm::getting_pin;
				return true;
			}
		);
	}

	void done_processing() {
		to_interface.send(interface::eject_card());
		state = &atm::waiting_for_card;
	}

public:
	atm(messaging::sender b, messaging::sender i) : to_bank(b), to_interface(i) {}

	void run() {
		state = &atm::waiting_for_card;
		try {
			while (true) {
				(this->*state)();
			}
		}
		catch (close_queue const&) {}
	}

	messaging::sender get_sender() {
		return inbox.get_sender();
	}

	void done() {
		get_sender().send(close_queue());
	}
};

class bank_machine {
	messaging::receiver inbox;
	unsigned balance = 199;
public:
	void run() {
		try {
			while (true) {
				auto msg = inbox.wait();
				std::visit(overloaded{
					[&](bank::verify_pin const& msg) {
						if (msg.pin == "1937")
							msg.atm_queue->send(bank::pin_verified());
						else
							msg.atm_queue->send(bank::pin_incorrect());
					},
					[&](bank::withdraw const& msg) {
						if (balance >= msg.amount) {
							balance -= msg.amount;
							msg.atm_queue->send(bank::withdrawal_approved());
						}
						else {
							msg.atm_queue->send(bank::withdrawal_denied());
						}
					},
					[&](bank::get_balance const& msg) {
						msg.atm_queue->send(bank::balance_received(balance));
					},
					[&](bank::withdrawal_processed const&) {
						// Transaction finalized
					},
					[&](bank::cancel_withdrawal const&) {
						// Logic to unlock funds
					},
					[&](close_queue const&) {
						throw close_queue();
					},
					[](auto const&) {}
					}, msg);
			}
		}
		catch (close_queue const&) {}
	}

	messaging::sender get_sender() {
		return inbox.get_sender();
	}

	void done() {
		get_sender().send(close_queue());
	}
};

class interface_machine {
	messaging::receiver inbox;
public:
	void run() {
		try {
			while (true) {
				auto msg = inbox.wait();
				std::visit(overloaded{
					[&](interface::issue_money const& msg) {
						std::println("Hardware: Issuing {}", msg.amount);
					},
					[&](interface::display_insufficient_funds const&) {
						std::println("Screen: Insufficient funds");
					},
					[&](interface::display_enter_pin const&) {
						std::println("Screen: Please enter your PIN (0-9)");
					},
					[&](interface::display_enter_card const&) {
						std::println("Screen: Please enter your card (Press I)");
					},
					[&](interface::display_balance const& msg) {
						std::println("Screen: Balance is {}", msg.amount);
					},
					[&](interface::display_withdrawal_options const&) {
						std::println("Screen: Withdraw 50 (w) | Balance (b) | Cancel (c)");
					},
					[&](interface::display_withdrawal_cancelled const&) {
						std::println("Screen: Transaction Cancelled");
					},
					[&](interface::display_pin_incorrect_message const&) {
						std::println("Screen: PIN Incorrect");
					},
					[&](interface::eject_card const&) {
						std::println("Hardware: Ejecting card");
					},
					[&](close_queue const&) { throw close_queue(); },
					[](auto const&) {}
					}, msg);
			}
		}
		catch (close_queue&) {}
	}

	messaging::sender get_sender() {
		return inbox.get_sender();
	}

	void done() {
		get_sender().send(close_queue());
	}
};

int main() {
	bank_machine bank;
	interface_machine ui_hardware;
	atm machine(bank.get_sender(), ui_hardware.get_sender());

	std::thread bank_thread(&bank_machine::run, &bank);
	std::thread ui_thread(&interface_machine::run, &ui_hardware);
	std::thread atm_thread(&atm::run, &machine);

	messaging::sender atm_inbox(machine.get_sender());

	bool quit = false;
	while (!quit) {
		char c = getchar();
		switch (c) {
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			atm_inbox.send(interface::digit_pressed(c));
			break;
		case 'b':
			atm_inbox.send(interface::balance_pressed());
			break;
		case 'w':
			atm_inbox.send(interface::withdraw_pressed(50));
			break;
		case 'c':
			atm_inbox.send(interface::cancel_pressed());
			break;
		case 'q':
			quit = true;
			break;
		case 'i':
			atm_inbox.send(interface::card_inserted("acc1234"));
			break;
		}
	}

	bank.done();
	machine.done();
	ui_hardware.done();

	atm_thread.join();
	bank_thread.join();
	ui_thread.join();
}