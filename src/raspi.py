from queue import Queue

from measure.sensor import Cap1188Sensor
from transport.udp import UDPSender

interrupt_queue = Queue()

sensor = Cap1188Sensor(interrupt_queue, 22, 4, "1.28ms", "70ms")

sensor.start()
print("Subscribed to CAP1188 interrupts!\n")

sender = UDPSender("penderecki.local", 5678)

try:
    while True:
        interrupt = interrupt_queue.get()
        sender.send(interrupt)
except KeyboardInterrupt:
    print("\nExiting..!")
