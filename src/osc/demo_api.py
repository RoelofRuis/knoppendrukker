from pythonosc.udp_client import SimpleUDPClient


class PdDemoApi:
    def __init__(self, ip: str = "127.0.0.1", port: int = 8765):
        self.client = SimpleUDPClient(ip, port)

    def play(self, freq: float, vol: float, dur: float):
        self.client.send_message("/play", (freq, vol, dur))

    def kick(self):
        self.client.send_message("/kick", ())