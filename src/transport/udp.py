import socket
import struct

from contract import Interrupt

EVENT = struct.Struct("!di")

class UDPSender:
    def __init__(self, address, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.address = address
        self.port = port

    def send(self, interrupt: Interrupt):
        packet = EVENT.pack(interrupt.timestamp, interrupt.pins)
        self.sock.sendto(packet, (self.address, self.port))

class UDPReceiver:
    def __init__(self, host: str = "0.0.0.0", port: int = 5678):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._socket.bind((host, port))

    def receive(self) -> Interrupt:
        packet, addr = self._socket.recvfrom(EVENT.size)
        timestamp, pins = EVENT.unpack(packet)
        return Interrupt(timestamp, pins)

    def close(self) -> None:
        if self._socket is not None:
            self._socket.close()

        self._socket = None