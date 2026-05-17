#pragma once
#include <map>
#include <list>
#include <unordered_map>
#include <optional>
#include <functional>
#include <string>
#include <chrono>
#include "order_book.h"

namespace ob {

class ArrayOrderBook : public IOrderBook {

public:
    ArrayOrderBook(double min_price, double max_price, double tick_size):
    min_price(min_price), max_price(max_price), tick_size(tick_size),
    best_bid_idx(-1), best_ask_idx(-1) {
        if (tick_size <= 0) throw std::invalid_argument("tick_size must be > 0");
        if (min_price >= max_price) throw std::invalid_argument("min_price must be < max_price");

        num_slots = static_cast<int>(std::round((max_price - min_price) / tick_size)) + 1;
        slots.resize(num_slots, nullptr);
    }

    void addOrder(int order_id, Side side, int shares, int limit_price, double entry_time) {
        int idx = priceToIdx(limit_price);
        if (idx < 0) throw std::out_of_range("Price out of book range");

        if (!slots[idx]) {
            slots[idx] = new Limit(limit_price);
        }

        Limit* lim = slots[idx];
        lim->orders.emplace_back(order_id, side, shares, limit_price, entry_time);
        auto list_it = std::prev(lim->orders.end());
        lim->size += 1;
        lim->total_volume += shares;
        orders_by_id.try_emplace(order_id, Entry{idx, list_it, side});

        if (side == Side::BUY) {
            if (best_bid_idx < 0 || idx > best_bid_idx) best_bid_idx = idx;
        } else {
            if (best_ask_idx < 0 || idx < best_ask_idx) best_ask_idx = idx;
        }
    }

    void cancelOrder(int order_id) {
        auto o_it = orders_by_id.find(order_id);
        if (o_it == orders_by_id.end()) return;

        int slot_idx = o_it->second.slot_idx;
        auto list_it = o_it->second.it;
        Side side = o_it->second.side;

        Limit* lim = slots[slot_idx];
        lim->total_volume -= list_it->shares;
        lim->size -= 1;
        lim->orders.erase(list_it);
        orders_by_id.erase(o_it);

        removeLimitIfEmpty(slot_idx, side);
    }

    std::vector<Trade> submitOrder(int order_id, Side side, int shares, int limit_price, double entry_time) {
        std::vector<Trade> trades;
        int remaining = shares;

        while (remaining > 0) {
            int resting_idx = (side == Side::BUY) ? best_ask_idx : best_bid_idx;
            if (resting_idx < 0) break;
            Limit* resting_limit = slots[resting_idx];
            if (!resting_limit || resting_limit->orders.empty()) break;

            // check if match can occur based on MM's offer
            int incoming_idx = priceToIdx(limit_price);
            if (side == Side::BUY && resting_idx > incoming_idx) break;
            if (side == Side::SELL && resting_idx < incoming_idx) break;

            Order& resting_order = resting_limit->orders.front();
            int matched_shares = std::min(remaining, resting_order.shares);

            Trade tr;
            tr.shares = matched_shares;
            tr.price = resting_limit->limit_price;
            tr.incoming_side = side;
            
            if (side == Side::BUY) {
                tr.buy_order_id = order_id;
                tr.sell_order_id = resting_order.order_id;
            } else {
                tr.buy_order_id = resting_order.order_id;
                tr.sell_order_id = order_id;
            }

            Side resting_side = (side == Side::BUY) ? Side::SELL : Side::BUY;
            applyTrade(resting_idx, matched_shares, resting_order.order_id, resting_side);
            remaining -= matched_shares;
            tr.fill_type = (remaining == 0) ? FillType::FULL : FillType::PARTIAL;
            trades.push_back(tr);
        }

        if (remaining) {
            addOrder(order_id, side, remaining, limit_price, entry_time);
        }
        return trades;
    }

    std::optional<int> bestBid() const {
        if (best_bid_idx < 0) return std::nullopt;
        return static_cast<int>(std::round(idxToPrice(best_bid_idx)));
    }

    std::optional<int>bestAsk() const {
        if (best_ask_idx < 0) return std::nullopt;
        return static_cast<int>(std::round(idxToPrice(best_ask_idx)));
    }

private:
    double min_price;
    double max_price;
    double tick_size;
    int    num_slots;

    // index -> Limit* array
    std::vector<Limit*> slots;

    // Best bid/ask slot indices (-1 when not existing)
    int best_bid_idx;
    int best_ask_idx;

    struct Entry {
        int slot_idx;
        std::list<Order>::iterator it;
        Side side;
    };
    std::unordered_map<int, Entry> orders_by_id;

    int priceToIdx(double price) const {
        int idx = static_cast<int>(std::round((price-min_price) / tick_size));
        if (idx < 0 || idx >= num_slots) return -1;
        return idx;
    }

    double idxToPrice(int idx) const {
        return min_price + idx * tick_size;
    }

    int updateBestBidAfterRemoval(int from_idx) {
        for (int i = from_idx; i>=0; i--) {
            if (slots[i] && !slots[i]->orders.empty()) {
                return i;
            }
        }
        return -1;
    }

    int updateBestAskAfterRemoval(int from_idx) {
        for (int i=from_idx; i<num_slots; i++) {
            if (slots[i] && !slots[i]->orders.empty()) {
                return i;
            }
        }
        return -1;
    }

    void removeLimitIfEmpty(int slot_idx, Side side) {
        Limit* lim = slots[slot_idx];
        if (!lim || !lim->orders.empty()) return;

        delete lim;
        slots[slot_idx] = nullptr;

        if (side == Side::BUY && slot_idx == best_bid_idx) {
            best_bid_idx = updateBestBidAfterRemoval(slot_idx-1);
        } else if (side == Side::SELL && slot_idx == best_ask_idx) {
            best_ask_idx = updateBestAskAfterRemoval(slot_idx+1);
        }
    }

    void applyTrade(int slot_idx, int qty, int order_id, Side resting_side) {
        Limit* lim = slots[slot_idx];
        if (!lim || lim->orders.empty()) return;

        auto o_it = orders_by_id.find(order_id);
        if (o_it == orders_by_id.end()) return;

        auto list_it = o_it->second.it;
        Order& order = *list_it;

        int matched_shares = std::min(qty, order.shares);
        order.shares -= matched_shares;
        lim->total_volume -= matched_shares;

        if (order.shares == 0) {
            lim->orders.erase(list_it);
            lim->size -= 1;
            orders_by_id.erase(o_it);
            removeLimitIfEmpty(slot_idx, resting_side);
        }
    }
};
}