
#pragma once

#include "Order.h"
#include "OrderType.h"

#include <iostream>
#include <queue>
#include <vector>
#include <unordered_map>

class OrderBook {
    public:
        void addOrder(Side side, OrderType type, double price, int quantity) {
            int orderId = ++orderIdCounter;
            Order order(orderId, side, type, price, quantity);
            orders[orderId] = order;

            if (side == Side::Buy) {
                buyOrders.push(order);
            } else {
                sellOrders.push(order);
            }

            matchOrders();
        }

        void matchOrders() {
            while (!buyOrders.empty() && !sellOrders.empty()) {
                Order buyOrder = buyOrders.top();
                Order sellOrder = sellOrders.top();

                if (buyOrder.getPrice() >= sellOrder.getPrice()) {
                    int tradedQuantity = std::min(buyOrder.getRemainingQuantity(), sellOrder.getRemainingQuantity());
                    std::cout << "Matched order with quantity: " << tradedQuantity << " at price: " << sellOrder.getPrice() << "\n";

                    buyOrders.pop();
                    sellOrders.pop();

                    buyOrder.remainingQuantity_ -= tradedQuantity;
                    sellOrder.remainingQuantity_ -= tradedQuantity;

                    if (buyOrder.quantity > 0) {
                        buyOrders.push(buyOrder);
                    }
                    if (sellOrder.quantity > 0) {
                        sellOrders.push(sellOrder);
                    }
                } else {
                    break;
                }
            }
        }

        void printOrders() {
            std::cout << "Buy orders: \n";
            printQueue(buyOrders);

            std::cout << "Sell orders: \n";
            printQueue(sellOrders);
        }

    private:
        std::priority_queue<Order> buyOrders;
        std::priority_queue<Order> sellOrders;
        std::unordered_map<int, Order> orders;
        int orderIdCounter = 0;

        void printQueue(std::priority_queue<Order> orders) {
            while (!orders.empty()) {
                Order order = orders.top();
                std::cout << "ID: " << order.id << " | Price: " << order.price << " | Quantity: " << order.quantity << "\n";
                orders.pop();
            }
        }
};
