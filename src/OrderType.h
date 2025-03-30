#pragma once

enum class OrderType {
    MarketOrder,
    GTC,
    DayOrder,
    LimitOrder,
    StopOrder,
    StopLossOrder,
};