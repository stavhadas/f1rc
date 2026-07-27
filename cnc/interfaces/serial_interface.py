import serial

from .interface import Interface
from protocols.protocol import Protocol

class SerialInterface(Interface):
    def __init__(self, name : str, protocols : list[Protocol], port : str, baudrate : int, timeout : float = 1.0):
        super().__init__(name, protocols)
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial = None

    def start(self):
        self.serial = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
        super().start()

    def send_bytes(self, bytes):
        """ Send bytes through the interface. """
        self.serial.write(bytes)

    def receive_bytes(self):
        """ Receive bytes from the interface. """
        # Block up to `timeout` for at least 1 byte, then drain whatever else
        # is already buffered so multi-byte chunks aren't read one at a time.
        return self.serial.read(max(1, self.serial.in_waiting))

    def stop(self):
        super().stop()
        self.serial.close()
