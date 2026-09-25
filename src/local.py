from osc.demo_api import PdDemoApi
from transport.udp import UDPReceiver

receiver = UDPReceiver("0.0.0.0", 5678)

api = PdDemoApi()

try:
    while True:
        msg = receiver.receive()

        if msg.pins & 1:
            api.kick()
            print("Play kick")
            continue

        freq = 0
        for i in range(8):
            if msg.pins & (1 << i):
                freq += 100 * i

        print(f"Playing {freq} Hz")
        api.play(freq, 0.1, 500)

except KeyboardInterrupt:
    print("\nExiting...")
    pass
