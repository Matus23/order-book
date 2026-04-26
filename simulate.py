from order_book import OrderBook
from events import EventGenerator, AddOrder, CancelOrder

book = OrderBook()
gen = EventGenerator(seed=123)

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