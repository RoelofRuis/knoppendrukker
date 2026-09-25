import time

import board
from queue import Queue

from adafruit_cap1188.i2c import CAP1188_I2C
from gpiozero import Button

from contract import Interrupt


class Cap1188Sensor:
    _sensor: CAP1188_I2C
    _int_pin: Button

    def __init__(self, interrupt_pin: int, queue: Queue):
        self.interrupt_pin = interrupt_pin
        self.is_started = False
        self.queue = queue

    def handle_interrupt(self):
        touched_pins = self._sensor.touched_pins
        print("Interrupt")
        for i in range(1, 9):
            raw = self._sensor[i].raw_value
            print(f"Value {i}: {raw}")
        state = sum(pin << i for i, pin in enumerate(touched_pins))
        self.queue.put(Interrupt(
            timestamp=time.monotonic(),
            pins=state
        ))
        print("\n")

    def start(self):
        if self.is_started:
            return

        self.is_started = True

        i2c = board.I2C()
        self._sensor = CAP1188_I2C(i2c)

        self._sensor.alert_polarity = True
        self._sensor.interrupt_on_release = False

        for i in range(1, 9):
            self._sensor[i].interrupt_enabled = True
            self._sensor[i].interrupt_repeat = False

        self._sensor.clear_interrupt()

        self._int_pin = Button(self.interrupt_pin, pull_up=True)
        self._int_pin.when_pressed = self.handle_interrupt

    def stop(self):
        if not self.is_started:
            return

        self.is_started = False
        self._int_pin.close()

