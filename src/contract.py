from dataclasses import dataclass


@dataclass(frozen=True)
class Interrupt:
    timestamp: float
    pins: int
