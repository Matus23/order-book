#pragma once

#include "Side.h"
#include "OrderType.h"

#include <iostream>
#include <queue>
#include <vector>
#include <unordered_map>

class Order {
    public:
        Order():
            orderId_(0), 
            side_(Side::Buy), 
            orderType_(OrderType::MarketOrder), 
            price_(0.0), 
            initialQuantity_(0), 
            remainingQuantity_(0) {}
        
        Order(int orderId, Side side, OrderType type, double price, int quantity): 
            orderId_(orderId),
            side_(side),
            orderType_(type),
            price_(price),
            initialQuantity_(quantity),
            remainingQuantity_(quantity) {}

        int getOrderId() const {return orderId_;}
        OrderType getOrderType() const {return orderType_;}
        double getPrice() const {return price_;}
        int getInitialQuantity() const {return initialQuantity_;}
        int getRemainingQuantity() const {return remainingQuantity_;}
        bool isFilled() const {return getRemainingQuantity() == 0;}

        void setRemainingQuantity(int quantity) {
            remainingQuantity_ = quantity;
        }

        // Operator overloading for priority queue (max heap for buys, min heap for sells)
        bool operator<(const Order& other) const {
            if (side_ == Side::Buy) {
                return price_ < other.getPrice(); // Higher price has higher priority
            } else {
                return price_ > other.getPrice(); // Lower price has higher priority
            }
        }

    private:
        int orderId_;
        Side side_;
        OrderType orderType_;
        double price_;
        int initialQuantity_;
        int remainingQuantity_;
};