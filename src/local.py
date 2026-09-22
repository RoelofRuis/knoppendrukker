from pythonosc.udp_client import SimpleUDPClient

from transport.udp import UDPReceiver

receiver = UDPReceiver("0.0.0.0", 5678)

IP = "127.0.0.1"
PORT = 8765

client = SimpleUDPClient(IP, PORT)

try:
    while True:
        msg = receiver.receive()

        freq = 0
        for i in range(8):
            if msg.pins & (1 << i):
                freq += 100 * i

        print(f"Playing {freq} Hz")
        client.send_message("/play", (freq, 0.1))

except KeyboardInterrupt:
    print("\nExiting...")
    pass
