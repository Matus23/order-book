import random
from dataclasses import dataclass
from enum import Enum, auto

class EventType(Enum):
    ADD = auto()
    CANCEL = auto()

class Side(Enum):
    BUY = 1
    SELL = -1


@dataclass(frozen=True)
class AddOrder:
    time: float
    order_id: int
    side: Side
    price: int
    qty: int

@dataclass(frozen=True)
class CancelOrder:
    time: float
    order_id: int

class EventGenerator:
    def __init__(
        self,
        seed: int = 42,
        start_price: int = 100,
        price_spread: int = 10,
        max_qty: int = 1000,
        cancel_prob: float = 0.3,
        dt_mean: float = 0.001,
    ):
        self.rng = random.Random(seed)
        self.mid_price = start_price
        self.price_spread = price_spread
        self.max_qty = max_qty
        self.cancel_prob = cancel_prob
        self.dt_mean = dt_mean

        self.current_time = 0.0
        self.next_order_id = 1
        self.live_orders = set()

    def _next_time(self) -> float:
        # Exponential inter-arrival (Poisson flow)
        dt = self.rng.expovariate(1.0 / self.dt_mean)
        self.current_time += dt
        return self.current_time

    def _random_price(self, side: Side) -> int:
        offset = self.rng.randint(0, self.price_spread)
        if side == Side.BUY:
            return self.mid_price - offset
        else:
            return self.mid_price + offset
        
    def _random_qty(self) -> int:
        # Heavy-ish tail
        return max(1, int(self.rng.expovariate(1 / (self.max_qty / 3))))

    def next_event(self):
        t = self._next_time()

        # Cancel only if there are live orders
        if self.live_orders and self.rng.random() < self.cancel_prob:
            order_id = self.rng.choice(tuple(self.live_orders))
            self.live_orders.remove(order_id)
            return CancelOrder(time=t, order_id=order_id)

        # Otherwise add a new order
        side = Side.BUY if self.rng.random() < 0.5 else Side.SELL
        price = self._random_price(side)
        qty = self._random_qty()

        order_id = self.next_order_id
        self.next_order_id += 1
        self.live_orders.add(order_id)

        return AddOrder(
            time=t,
            order_id=order_id,
            side=side,
            price=price,
            qty=qty,
        )

    def generate(self, n: int):
        for _ in range(n):
            yield self.next_event()