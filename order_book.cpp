#include <iostream>
#include <queue>
#include <vector>
#include <unordered_map>

enum class OrderType { BUY, SELL };

struct Order {
    int id;
    OrderType type;
    double price;
    int quantity;

    // Default Constructor
    Order(): 
        id(0), type(OrderType::BUY), price(0.0), quantity(0) {}
    
    // Paremeterized Constructor
    Order(int id, OrderType type, double price, int quantity): 
        id(id), type(type), price(price), quantity(quantity) {}

    // Operator overloading for priority queue (max heap for buys, min heap for sells)
    bool operator<(const Order& other) const {
        if (type == OrderType::BUY) {
            return price < other.price; // Higher price has higher priority
        } else {
            return price > other.price; // Lower price has higher priority
        }
    }
};


class OrderBook {
    private:
        std::priority_queue<Order> buyOrders;
        std::priority_queue<Order> sellOrders;
        std::unordered_map<int, Order> orders;
        int orderIdCounter = 0;

    public:
        void addOrder(OrderType type, double price, int quantity) {
            int orderId = ++orderIdCounter;
            Order order(orderId, type, price, quantity);
            orders[orderId] = order;

            if (type == OrderType::BUY) {
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

                if (buyOrder.price >= sellOrder.price) {
                    int tradedQuantity = std::min(buyOrder.quantity, sellOrder.quantity);
                    std::cout << "Matched order with quantity: " << tradedQuantity << " at price: " << sellOrder.price << "\n";

                    buyOrders.pop();
                    sellOrders.pop();

                    buyOrder.quantity -= tradedQuantity;
                    sellOrder.quantity -= tradedQuantity;

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
        void printQueue(std::priority_queue<Order> orders) {
            while (!orders.empty()) {
                Order order = orders.top();
                std::cout << "ID: " << order.id << " | Price: " << order.price << " | Quantity: " << order.quantity << "\n";
                orders.pop();
            }
        }
};

int main() {
    OrderBook book;
    book.addOrder(OrderType::BUY, 100.5, 10);
    book.addOrder(OrderType::SELL, 99.0, 5);
    book.addOrder(OrderType::SELL, 101.0, 15);
    book.addOrder(OrderType::BUY, 102.0, 8);

    // std::cout << "\nFinal Order Book:\n";
    book.printOrders();
    book.printOrders();

    return 0;
}
