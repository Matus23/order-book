#pragma once
#include <map>
#include <list>
#include <unordered_map>
#include <optional>
#include <functional>
#include <string>
#include <chrono>

namespace ob {

enum class Side {BUY, SELL};

enum class FillType {FULL, PARTIAL};

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
    FillType fill_type;
    Side incoming_side;
};

struct Limit {
    int limit_price;
    std::list<Order> orders; // FIFO queue
    int size = 0;
    long long total_volume = 0;

    explicit Limit(int p): limit_price(p) {}
};


class OrderBook {

private:
    std::map<int, Limit, std::greater<int>> bids;
    std::map<int, Limit> asks;

    struct Entry {
        Limit* limit;
        std::list<Order>::iterator it;
    };
    std::unordered_map<int, Entry> orders_by_id;

    template<typename MapType>
    void addOrderToTree(MapType &tree, int order_id, Side side, int shares, int limit_price, double entry_time) {
        auto it = tree.find(limit_price);
        if (it == tree.end()) {
            it = tree.emplace(limit_price, Limit(limit_price)).first;
        }
        Limit &lim = it->second;
        lim.orders.emplace_back(order_id, side, shares, limit_price, entry_time);
        auto list_it = std::prev(lim.orders.end());
        lim.size += 1;
        lim.total_volume += shares;
        orders_by_id.emplace(order_id, Entry{ &lim, list_it });
    }

public:
    OrderBook() = default;

    // Add order as a new Order into the book (no matching here)
    void addOrder(int order_id, Side side, int shares, int limit_price, double entry_time) {
        if (side == Side::BUY) {
            addOrderToTree(bids, order_id, side, shares, limit_price, entry_time);
        } else {
            addOrderToTree(asks, order_id, side, shares, limit_price, entry_time);
        }
    }

    // Cancel an existing order by id (no-op if not found)
    void cancelOrder(int order_id) {
        auto o_it = orders_by_id.find(order_id);
        if (o_it == orders_by_id.end()) return;
        Limit *lim = o_it->second.limit;
        auto list_it = o_it->second.it;
        lim->total_volume -= list_it->shares;
        lim->size -= 1;
        lim->orders.erase(list_it);
        orders_by_id.erase(o_it);

        // remove empty limit from its tree
        removeLimitIfEmpty(lim->limit_price);
    }


    // Submit order: try to match immediately; if any remainder, add to book
    // Returns vector of Trades executed (may be empty)
    std::vector<Trade> submitOrder(int order_id, Side side, int shares, int limit_price, double entry_time) {
        std::vector<Trade> trades;
        int remaining = shares;
        // keep executing against the inside until order fully filled or no crossing
        while (remaining > 0) {
            Limit *restingLimit = (side == Side::BUY) ? bestAskLimit() : bestBidLimit();
            // break if no match can be made
            if (!restingLimit) break;
            if (side == Side::BUY && restingLimit->limit_price > limit_price) break;
            if (side == Side::SELL && restingLimit->limit_price < limit_price) break;

            Order &oppOrder = restingLimit->orders.front();
            int take = std::min(remaining, oppOrder.shares);
            Trade tr;
            if (side == Side::BUY) {
                tr.buy_order_id = order_id;
                tr.sell_order_id = oppOrder.order_id;
            } else {
                tr.buy_order_id = oppOrder.order_id;
                tr.sell_order_id = order_id;
            }
            tr.price = restingLimit->limit_price;
            tr.shares = take;
            tr.incoming_side = side;
            // apply trade to book (reuse applyTrade helper)
            applyTrade(restingLimit, take, oppOrder.order_id);
            remaining -= take;
            tr.fill_type = (remaining == 0) ? FillType::FULL : FillType::PARTIAL;
            trades.push_back(tr);
        }

        if (remaining > 0) {
            // add the remaining as a resting order
            addOrder(order_id, side, remaining, limit_price, entry_time);
        }
        return trades;
    }

    // Execute one best-against-best match, return Trade if a match happened
    std::optional<Trade> executeBest() {
        auto bidIt = bids.begin();
        auto askIt = asks.begin();
        if (bidIt == bids.end() || askIt == asks.end()) return std::nullopt;
        if (bidIt->first < askIt->first) return std::nullopt; // no crossing

        Limit &buyL = bidIt->second;
        Limit &sellL = askIt->second;
        if (buyL.orders.empty() || sellL.orders.empty()) return std::nullopt;

        Order &buyOrder = buyL.orders.front();
        Order &sellOrder = sellL.orders.front();

        int traded = std::min(buyOrder.shares, sellOrder.shares);
        Trade tr;
        tr.shares = traded;
        tr.buy_order_id = buyOrder.order_id;
        tr.sell_order_id = sellOrder.order_id;
        // price convention: use sell (aggressed) price
        tr.price = sellL.limit_price;

        // apply fills
        applyTrade(&buyL, traded, buyOrder.order_id);
        applyTrade(&sellL, traded, sellOrder.order_id);

        return tr;
    }

    // Queries
    std::optional<int> bestBid() const {
        if (bids.empty()) return std::nullopt;
        return bids.begin()->first;
    }
    std::optional<int> bestAsk() const {
        if (asks.empty()) return std::nullopt;
        return asks.begin()->first;
    }
    

private:
    void removeLimitIfEmpty(int price) {
        auto b = bids.find(price);
        if (b != bids.end() && b->second.orders.empty()) {
            bids.erase(b);
            return;
        }
        auto a = asks.find(price);
        if (a != asks.end() && a->second.orders.empty()) {
            asks.erase(a);
        }
    }

    Limit* bestAskLimit() {
        if (asks.empty()) return nullptr;
        return &asks.begin()->second;
    }
    Limit* bestBidLimit() {
        if (bids.empty()) return nullptr;
        return &bids.begin()->second;
    }

    // apply trade quantity to a specific limit's front order (identified by id)
    // This reduces shares and removes order+limit as necessary.
    void applyTrade(Limit* lim, int qty, int order_id) {
        if (!lim || lim->orders.empty()) return;
        Order &o = lim->orders.front();
        if (o.order_id != order_id) {
            // find via orders_by_id (shouldn't happen if caller identified correctly)
            auto it = orders_by_id.find(order_id);
            if (it == orders_by_id.end()) return;
            lim = it->second.limit;
            auto list_it = it->second.it;
            Order &oo = *list_it;
            int use = std::min(qty, oo.shares);
            oo.shares -= use;
            lim->total_volume -= use;
            if (oo.shares == 0) {
                lim->orders.erase(list_it);
                orders_by_id.erase(it);
            }
            removeLimitIfEmpty(lim->limit_price);
            return;
        }

        int use = std::min(qty, o.shares);
        o.shares -= use;
        lim->total_volume -= use;
        if (o.shares == 0) {
            // remove front
            orders_by_id.erase(o.order_id);
            lim->orders.pop_front();
            lim->size -= 1;
        }
        removeLimitIfEmpty(lim->limit_price);
    }
};

}