#include <iostream>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <random>
#include <deque>
#include <sstream>
#include "order_book.cpp"

// Minimal event generator (C++ version of Python EventGenerator)
class EventGenerator {

public:
    enum class EventType { ADD, CANCEL };
    struct Event {
        EventType type;
        int order_id;
        ob::Side side;
        int shares;
        int price;
        double time;
    };

    EventGenerator(int seed = 123, double cancel_prob = 0.3, int price_min = 95, int price_max = 105)
        : gen(seed),
          dist_side(0, 1),
          dist_qty(10, 100),
          dist_price(price_min, price_max),
          cancel_prob(cancel_prob),
          order_counter(0) {}

    Event next() {
        // side: 50/50 BUY/SELL
        ob::Side side = (dist_side(gen) < 0.5) ? ob::Side::BUY : ob::Side::SELL;
        
        // shares: uniform [10, 100]
        int shares = dist_qty(gen);
        
        // price: uniform [price_min, price_max]
        int price = dist_price(gen);
        
        // event type: ADD with prob (1 - cancel_prob), CANCEL otherwise
        EventType etype = (dist_event(gen) > cancel_prob) ? EventType::ADD : EventType::CANCEL;
        
        int oid = order_counter++;
        return Event{etype, oid, side, shares, price, 0.0};
    }

    // track active order IDs for realistic cancel generation
    void recordAddOrder(int order_id) {
        active_orders.push_back(order_id);
        if (active_orders.size() > 1000) {
            active_orders.pop_front(); // keep window of ~1000 recent orders
        }
    }

    std::optional<int> getRandomActiveOrder() {
        if (active_orders.empty()) return std::nullopt;
        int idx = static_cast<int>(dist_side(gen) * active_orders.size());
        return active_orders[idx];
    }

private:
    std::mt19937 gen;
    std::uniform_real_distribution<> dist_event;     // [0, 1] for event type
    std::uniform_real_distribution<> dist_side;      // [0, 1] for side
    std::uniform_int_distribution<> dist_qty;        // [10, 100] for qty
    std::uniform_int_distribution<> dist_price;      // [price_min, price_max] for price
    double cancel_prob;
    int order_counter;
    std::deque<int> active_orders;

};

class Logger {
public:
    Logger(const std::string& filename) : enabled(true) {
        file.open(filename, std::ios::app);
    }

    ~Logger() { if (file.is_open()) file.close(); }

    void disable() { enabled = false; }
    void enable() { enabled = true; }

    void logEvent(const std::string& event_type, int order_id, ob::Side side, int qty, int price) {
        if (!enabled) return;
        file << "[" << getCurrentDateTime() <<  "]" << event_type << " order_id=" << order_id
             << " side=" << (side == ob::Side::BUY ? "BUY" : "SELL")
             << " qty=" << qty << " price=" << price << "\n";
        file.flush();
    }

    void logTrade(int buy_id, int sell_id, int qty, int price, ob::Side side, ob::FillType fillType) {
        if (!enabled) return;
        file << "[" << getCurrentDateTime() <<  "]" << "TRADE buy_order_id=" 
             << buy_id << " sell_order_id=" << sell_id
             << " side=" << (side == ob::Side::BUY ? "BUY" : "SELL")
             << " qty=" << qty << " price=" << price 
             << " fill=" << (fillType == ob::FillType::FULL ? "FULL": "PARTIAL") << "\n";
        file.flush();
    }

private:
    std::ofstream file;
    bool enabled;

    std::string getCurrentDateTime() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
};

int main(int argc, char** argv) {
    int n_events = 100'000;
    int seed = 123;
    double cancel_prob = 0.3;
    int price_min = 95;
    int price_max = 105;
    bool disable_logging = false;

    // parse args
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--events") == 0 && i + 1 < argc) {
            n_events = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--cancel-prob") == 0 && i + 1 < argc) {
            cancel_prob = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--price-min") == 0 && i + 1 < argc) {
            price_min = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--price-max") == 0 && i + 1 < argc) {
            price_max = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--disable-logging") == 0) {
            disable_logging = true;
        }
    }

    ob::OrderBook book;
    EventGenerator gen(seed, cancel_prob, price_min, price_max);
    Logger logger("events.log");

    if (disable_logging) {
        logger.disable();
    }

    auto start = std::chrono::high_resolution_clock::now();

    int trade_count = 0;
    for (int i = 0; i < n_events; ++i) {
        auto evt = gen.next();
        if (evt.type == EventGenerator::EventType::ADD) {
            gen.recordAddOrder(evt.order_id);
            int inc_shares = evt.shares;
            auto trades = book.submitOrder(evt.order_id, evt.side, evt.shares, evt.price, evt.time);
            trade_count += trades.size();
            for (const auto& t : trades) {
                logger.logTrade(t.buy_order_id, t.sell_order_id, t.shares, t.price, t.incoming_side, t.fill_type);
            }
            logger.logEvent("ADD", evt.order_id, evt.side, evt.shares, evt.price);
        } else {
            auto oid = gen.getRandomActiveOrder();
            if (oid) {
                book.cancelOrder(*oid);
                logger.logEvent("CANCEL", *oid, ob::Side::BUY, 0, 0);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();

    std::cout << "events=" << n_events << " elapsed_s=" << elapsed
              << " events/s=" << static_cast<long long>(n_events / elapsed)
              << " trades=" << trade_count << " cancel_prob=" << cancel_prob << "\n";

    return 0;
}