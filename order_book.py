from enum import Enum
from datetime import datetime
import logging_utils

class Side(str, Enum):
    BUY = "BUY"
    SELL = "SELL"


class Order:
    def __init__(self, order_id, side, shares, limit_price, entry_time):
        self.order_id = order_id
        self.side = side
        self.shares = shares
        self.limit_price = limit_price
        self.entry_time = entry_time
        self.created_time = datetime.utcnow().isoformat()
        self.filled_time = None

        # Doubly linked list pointers (within a Limit)
        self.prev_order = None
        self.next_order = None

        # Back pointer to parent Limit
        self.parent_limit = None

    def __repr__(self):
        return f"Order(id={self.order_id}, shares={self.shares})"


class Limit:
    def __init__(self, limit_price):
        self.limit_price = limit_price

        # Aggregates
        self.size = 0
        self.total_volume = 0

        # BST pointers
        self.parent = None
        self.left = None
        self.right = None

        # FIFO queue of orders
        self.head_order = None
        self.tail_order = None

    def append_order(self, order: Order):
        """Append order to FIFO queue (O(1))"""
        order.parent_limit = self

        if self.tail_order is None:
            self.head_order = self.tail_order = order
        else:
            self.tail_order.next_order = order
            order.prev_order = self.tail_order
            self.tail_order = order

        self.size += 1
        self.total_volume += order.shares

    def remove_order(self, order: Order):
        """Remove order from FIFO queue (O(1))"""
        if order.prev_order:
            order.prev_order.next_order = order.next_order
        else:
            self.head_order = order.next_order

        if order.next_order:
            order.next_order.prev_order = order.prev_order
        else:
            self.tail_order = order.prev_order

        self.size -= 1
        self.total_volume -= order.shares

        order.prev_order = None
        order.next_order = None
        order.parent_limit = None

    def __repr__(self):
        return f"Limit(price={self.limit_price}, size={self.size}, vol={self.total_volume})"


class OrderBook:
    def __init__(self):
        # Root nodes
        self.buy_tree = None
        self.sell_tree = None

        # Inside market
        self.best_bid = None
        self.best_ask = None

        # Hash maps
        self.orders_by_id = {}
        self.limits_by_price = {}

    # -------------------------
    # BST helpers
    # -------------------------

    def _bst_insert(self, root, limit, side):
        if root is None:
            return limit

        if limit.limit_price < root.limit_price:
            root.left = self._bst_insert(root.left, limit, side)
            root.left.parent = root
        else:
            root.right = self._bst_insert(root.right, limit, side)
            root.right.parent = root

        return root

    def _bst_min(self, node):
        while node.left:
            node = node.left
        return node

    def _bst_max(self, node):
        while node.right:
            node = node.right
        return node

    # -------------------------
    # Core operations
    # -------------------------

    def add_order(self, order_id, side, shares, limit_price, entry_time):
        if order_id in self.orders_by_id:
            raise ValueError("Duplicate order ID")

        order = Order(order_id, side, shares, limit_price, entry_time)
        logging_utils.log_event(
            "ADD",
            time=order.entry_time,
            order_id=order.order_id,
            side=order.side,
            price=order.limit_price,
            qty=order.shares,
        )

        # Get or create limit
        limit = self.limits_by_price.get(limit_price)
        is_new_limit = False

        if not limit:
            limit = Limit(limit_price)
            self.limits_by_price[limit_price] = limit
            is_new_limit = True

            if side == Side.BUY:
                self.buy_tree = self._bst_insert(self.buy_tree, limit, side)
                if self.best_bid is None or limit_price > self.best_bid.limit_price:
                    self.best_bid = limit
            else:
                self.sell_tree = self._bst_insert(self.sell_tree, limit, side)
                if self.best_ask is None or limit_price < self.best_ask.limit_price:
                    self.best_ask = limit

        limit.append_order(order)
        self.orders_by_id[order_id] = order

    def cancel_order(self, order_id):
        order = self.orders_by_id.get(order_id)
        if not order:
            logging_utils.log_event("CANCEL_IGNORED", order_id=order_id)
            return

        logging_utils.log_event("CANCEL", order_id=order_id, price=order.limit_price,
            side=order.side, remaining_qty=order.shares)
        limit = order.parent_limit
        limit.remove_order(order)
        del self.orders_by_id[order_id]

        # Note: limit removal from BST omitted for clarity
        # (would be O(log M), rarely triggered)

    def _can_match(self, limit_price, side):
        if side == Side.BUY:
            return self.best_ask and limit_price >= self.best_ask.limit_price
        else:
            return self.best_bid and limit_price <= self.best_bid.limit_price

    def submit_order(self, order_id, side, shares, limit_price, entry_time):
        """Tries to match new order. If unsuccessful or partially matched,
            adds rest of the order to Order Book"""
        resting_side = self.best_ask if side == Side.BUY else self.best_bid
        while shares > 0 and self._can_match(limit_price, side) and resting_side.head_order:
            traded = min(shares, resting_side.head_order.shares)
            resting_side.head_order.shares -= traded
            trade_price = resting_side.limit_price
            if side == Side.BUY:
                buy_order_id = order_id
                sell_order_id = self.best_ask.head_order.order_id
            else:
                buy_order_id = self.best_bid.head_order.order_id
                sell_order_id = order_id

            if resting_side.head_order.shares == 0:
                del_order_id = resting_side.head_order.order_id
                resting_side.remove_order(resting_side.head_order)
                del self.orders_by_id[del_order_id]

            shares -= traded
            logging_utils.log_trade(
                time=datetime.utcnow().isoformat(),
                buy_order_id=buy_order_id,
                sell_order_id=sell_order_id,
                qty=traded,
                price=trade_price,
                fill='PARTIAL' if shares > 0 else 'FULL')

        if shares > 0:
            self.add_order(order_id, side, shares, limit_price, entry_time)

    def execute_order(self, order_id):
        """OBSOLETE - REFACTOR submit_order
        Execute against inside market (one order, FIFO)"""
        buy_order = self.best_bid.head_order
        sell_order = self.best_ask.head_order
        traded = min(buy_order.shares, sell_order.shares)
        if buy_order.limit_price != sell_order.limit_price:
            logging_utils.log_trade(
                msg="Bid and ask prices do not match",
                buy_price=buy_order.limit_price,
                sell_price=sell_order.limit_price,
                qty=traded)

        buy_order.shares -= traded
        sell_order.shares -= traded

        self.best_bid.total_volume -= traded
        self.best_ask.total_volume -= traded

        logging_utils.log_trade(
                time=datetime.utcnow().isoformat(),
                buy_order_id=buy_order.order_id,
                sell_order_id=sell_order.order_id,
                qty=traded,
                price=buy_order.limit_price)

        if buy_order.shares == 0:
            self.best_bid.remove_order(buy_order)
            del self.orders_by_id[buy_order.order_id]

        if sell_order.shares == 0:
            self.best_ask.remove_order(sell_order)
            del self.orders_by_id[sell_order.order_id]

        return traded

    # -------------------------
    # Queries
    # -------------------------

    def get_best_bid(self):
        return self.best_bid.limit_price if self.best_bid else None

    def get_best_ask(self):
        return self.best_ask.limit_price if self.best_ask else None

    def get_volume_at_price(self, price):
        limit = self.limits_by_price.get(price)
        return limit.total_volume if limit else 0
