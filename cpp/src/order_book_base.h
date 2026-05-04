#pragma once
#include <vector>
#include <list>
#include <optional>
#include <string>
#include <chrono>

namespace ob {

// -------------------------
// Enums
// -------------------------

enum class Side { BUY, SELL };

enum class FillType { FULL, PARTIAL };

// -------------------------
// Core structs
// -------------------------

struct Order {
    int order_id;
    Side side;
    int shares;
    int limit_price;
    double entry_time;
    std::string created_time;

    Order(int id, Side s, int sh, int price, double t)
        : order_id(id), side(s), shares(sh), limit_price(price), entry_time(t) {
        created_time = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count());
    }
};

struct Trade {
    int shares;
    int buy_order_id;
    int sell_order_id;
    int price;
    FillType fill_type;     // FULL if the incoming order was fully matched at this trade, PARTIAL otherwise
    Side incoming_side;     // side of the order that triggered this trade
};

struct Limit {
    int limit_price;
    std::list<Order> orders; // FIFO queue
    int size = 0;
    long long total_volume = 0;

    explicit Limit(int p) : limit_price(p) {}
};

// -------------------------
// Abstract interface
// -------------------------

class IOrderBook {
public:
    virtual ~IOrderBook() = default;

    // Add a resting order directly (no matching)
    virtual void addOrder(int order_id, Side side, int shares, int limit_price, double entry_time) = 0;

    // Cancel an existing order by ID (no-op if not found)
    virtual void cancelOrder(int order_id) = 0;

    // Submit an order: match immediately where possible, rest the remainder
    virtual std::vector<Trade> submitOrder(int order_id, Side side, int shares, int limit_price, double entry_time) = 0;

    // Queries
    virtual std::optional<int> bestBid() const = 0;
    virtual std::optional<int> bestAsk() const = 0;
};

} // namespace ob
