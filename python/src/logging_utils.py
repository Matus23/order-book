import logging
import json
from datetime import datetime
from enum import Enum

def setup_logger(name, filename, level=logging.INFO):
    logger = logging.getLogger(name)
    logger.setLevel(level)

    handler = logging.FileHandler(filename)
    handler.setLevel(level)

    formatter = logging.Formatter(
        '%(asctime)s %(levelname)s %(message)s'
    )
    handler.setFormatter(formatter)

    logger.addHandler(handler)
    logger.propagate = False
    return logger


# Core logs
event_logger = setup_logger("events", "../logs/events.log")
trade_logger = setup_logger("trades", "../logs/trades.log")
snapshot_logger = setup_logger("snapshots", "../logs/snapshots.log")
error_logger = setup_logger("errors", "../logs/errors.log")


def log_event(event_type, **kwargs):
    payload = {"type": event_type, **kwargs}
    clean_payload = {
        (k.value if isinstance(k, Enum) else k): (v.value if isinstance(v, Enum) else v)
        for k, v in payload.items()
    }
    event_logger.info(json.dumps(clean_payload))


def log_trade(**kwargs):
    trade_logger.info(json.dumps(kwargs))


def log_snapshot(snapshot_dict):
    snapshot_logger.info(json.dumps(snapshot_dict))


def log_error(msg, **kwargs):
    error_logger.error(json.dumps({"msg": msg, **kwargs}))
