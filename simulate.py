from order_book import OrderBook, Side
from events import EventGenerator, AddOrder, CancelOrder
import logging_utils
from datetime import datetime

book = OrderBook()
gen = EventGenerator(seed=123)

def match_book(book: OrderBook):
    """Match top-of-book until no crossing remains, logging trades."""
    while book.best_bid and book.best_ask and book.best_bid.limit_price >= book.best_ask.limit_price:
        buy_order = book.best_bid.head_order
        sell_order = book.best_ask.head_order
        if not buy_order or not sell_order:
            break

        buy_id = buy_order.order_id
        sell_id = sell_order.order_id
        # choose aggressor/ask price as trade price (simple rule)
        price = book.best_ask.limit_price

        traded = book.execute_best()
        if traded:
            logging_utils.log_trade(
                time=datetime.utcnow().isoformat(),
                buy_order_id=buy_id,
                sell_order_id=sell_id,
                qty=traded,
                price=price,
            )

for event in gen.generate(10_000):
    if isinstance(event, AddOrder):
        book.add_order(
            order_id=event.order_id,
            side=event.side,
            shares=event.qty,
            limit_price=event.price,
            entry_time=event.time,
        )
    elif isinstance(event, CancelOrder):
        book.cancel_order(event.order_id)
    match_book(book)