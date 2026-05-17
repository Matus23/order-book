#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <random>
#include <deque>
#include <memory>
#include <unordered_map>
#include "order_book/tree_order_book.h"
#include "order_book/array_order_book.h"

// -------------------------
// EventGenerator
// -------------------------

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
          dist_side(0.0, 1.0),
          dist_event(0.0, 1.0),
          dist_qty(10, 100),
          dist_price(price_min, price_max),
          cancel_prob(cancel_prob),
          order_counter(0) {}

    Event next() {
        ob::Side side = (dist_side(gen) < 0.5) ? ob::Side::BUY : ob::Side::SELL;
        int shares = dist_qty(gen);
        int price = dist_price(gen);
        EventType etype = (dist_event(gen) > cancel_prob) ? EventType::ADD : EventType::CANCEL;
        int oid = order_counter++;
        return Event{ etype, oid, side, shares, price, 0.0 };
    }

    void recordAddOrder(int order_id, ob::Side side) {
        active_orders.push_back(order_id);
        order_side_map[order_id] = side;
        if (active_orders.size() > 1000) {
            order_side_map.erase(active_orders.front());
            active_orders.pop_front();
        }
    }

    // Returns {order_id, side} of a random active order, or nullopt
    struct CancelTarget { int order_id; ob::Side side; };
    std::optional<CancelTarget> getRandomActiveOrder() {
        if (active_orders.empty()) return std::nullopt;
        int idx = static_cast<int>(dist_side(gen) * active_orders.size());
        int oid = active_orders[idx];
        return CancelTarget{ oid, order_side_map.at(oid) };
    }

private:
    std::mt19937 gen;
    std::uniform_real_distribution<> dist_event;
    std::uniform_real_distribution<> dist_side;
    std::uniform_int_distribution<> dist_qty;
    std::uniform_int_distribution<> dist_price;
    double cancel_prob;
    int order_counter;
    std::deque<int> active_orders;
    std::unordered_map<int, ob::Side> order_side_map;
};

// -------------------------
// Logger
// -------------------------

class Logger {
public:
    explicit Logger(const std::string& filename) : enabled(true) {
        file.open(filename, std::ios::app);
    }
    ~Logger() { if (file.is_open()) file.close(); }

    void disable() { enabled = false; }
    void enable()  { enabled = true;  }

    void logEvent(const std::string& event_type, int order_id, ob::Side side, int qty, int price) {
        if (!enabled) return;
        file << "[" << getCurrentDateTime() << "] "
             << event_type
             << " order_id=" << order_id
             << " side=" << (side == ob::Side::BUY ? "BUY" : "SELL")
             << " qty=" << qty
             << " price=" << price << "\n";
        file.flush();
    }

    void logTrade(int buy_id, int sell_id, int qty, int price,
                  ob::Side incoming_side, ob::FillType fill_type) {
        if (!enabled) return;
        file << "[" << getCurrentDateTime() << "] "
             << "TRADE"
             << " buy_order_id=" << buy_id
             << " sell_order_id=" << sell_id
             << " qty=" << qty
             << " price=" << price
             << " fill=" << (fill_type == ob::FillType::FULL ? "FULL" : "PARTIAL")
             << " incoming_side=" << (incoming_side == ob::Side::BUY ? "BUY" : "SELL")
             << "\n";
        file.flush();
    }

private:
    std::ofstream file;
    bool enabled;

    std::string getCurrentDateTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) % 1000;
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
};

// -------------------------
// main
// -------------------------

int main(int argc, char** argv) {
    int    n_events       = 100'000;
    int    seed           = 123;
    double cancel_prob    = 0.3;
    int    price_min      = 95;
    int    price_max      = 105;
    double tick_size      = 1.0;
    bool   disable_logging = false;
    std::string impl      = "tree"; // "tree" or "array"

    for (int i = 1; i < argc; ++i) {
        if      (std::strcmp(argv[i], "--events")          == 0 && i+1 < argc) n_events        = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--seed")            == 0 && i+1 < argc) seed             = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--cancel-prob")     == 0 && i+1 < argc) cancel_prob      = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--price-min")       == 0 && i+1 < argc) price_min        = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--price-max")       == 0 && i+1 < argc) price_max        = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--tick-size")       == 0 && i+1 < argc) tick_size        = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--impl")            == 0 && i+1 < argc) impl             = argv[++i];
        else if (std::strcmp(argv[i], "--disable-logging") == 0)               disable_logging  = true;
    }

    // Factory: construct correct implementation behind shared interface
    std::unique_ptr<ob::IOrderBook> book;
    if (impl == "array") {
        book = std::make_unique<ob::ArrayOrderBook>(price_min, price_max, tick_size);
    } else {
        book = std::make_unique<ob::TreeOrderBook>();
    }

    EventGenerator gen(seed, cancel_prob, price_min, price_max);
    Logger logger("events.log");
    if (disable_logging) logger.disable();

    auto start = std::chrono::high_resolution_clock::now();
    int trade_count = 0;

    for (int i = 0; i < n_events; ++i) {
        auto evt = gen.next();

        if (evt.type == EventGenerator::EventType::ADD) {
            gen.recordAddOrder(evt.order_id, evt.side);
            auto trades = book->submitOrder(evt.order_id, evt.side, evt.shares, evt.price, evt.time);
            trade_count += static_cast<int>(trades.size());

            for (const auto& t : trades) {
                logger.logTrade(t.buy_order_id, t.sell_order_id, t.shares, t.price,
                                t.incoming_side, t.fill_type);
            }
            logger.logEvent("ADD", evt.order_id, evt.side, evt.shares, evt.price);

        } else {
            auto target = gen.getRandomActiveOrder();
            if (target) {
                book->cancelOrder(target->order_id);
                logger.logEvent("CANCEL", target->order_id, target->side, 0, 0);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();

    std::cout << "impl=" << impl
              << " events=" << n_events
              << " elapsed_s=" << elapsed
              << " events/s=" << static_cast<long long>(n_events / elapsed)
              << " trades=" << trade_count
              << " cancel_prob=" << cancel_prob << "\n";

    return 0;
}
