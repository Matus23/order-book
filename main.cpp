#include "src/OrderBook.h"
#include "src/Side.h"

#include <iostream>
#include <queue>
#include <vector>
#include <unordered_map>


int main() {
    OrderBook book;
    book.addOrder(Side::Buy, OrderType::MarketOrder, 100.5, 10);
    book.addOrder(Side::Sell, OrderType::MarketOrder, 99.0, 5);
    book.addOrder(Side::Sell, OrderType::MarketOrder, 101.0, 15);
    book.addOrder(Side::Buy, OrderType::MarketOrder, 102.0, 8);

    // std::cout << "\nFinal Order Book:\n";
    book.printOrders();
    // book.printOrders();

    return 0;
}
