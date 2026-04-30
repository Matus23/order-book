import logging_utils
import argparse
import time

from order_book import OrderBook, Side
from events import EventGenerator, AddOrder, CancelOrder
from datetime import datetime

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

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", type=int, default=10_000, help="Number of events to generate")
    parser.add_argument("--seed", type=int, default=123, help="RNG seed")
    parser.add_argument("--disable-logging", action="store_true", help="Disable file logging during run")
    args = parser.parse_args()

    gen = EventGenerator(seed=args.seed)
    book = OrderBook()

    if args.disable_logging:
        logging_utils.event_logger.disabled = True
        logging_utils.trade_logger.disabled = True
        logging_utils.snapshot_logger.disabled = True
        logging_utils.error_logger.disabled = True

    start = time.perf_counter()
    for event in gen.generate(args.events):
        if isinstance(event, AddOrder):
            book.submit_order(
                order_id=event.order_id,
                side=event.side,
                shares=event.qty,
                limit_price=event.price,
                entry_time=event.time,
            )
        elif isinstance(event, CancelOrder):
            book.cancel_order(event.order_id)

    elapsed = time.perf_counter() - start
    print(f"events={args.events} elapsed_s={elapsed:.4f} events/s={args.events/elapsed:.0f}")
