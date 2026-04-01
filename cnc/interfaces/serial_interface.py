from .interface import Interface
from protocols.protocol import Protocol

class SerialInterface(Interface):
    def __init__(self, name : str, protocols : list[Protocol], port : str, baudrate : int):
        super().__init__(name, protocols)
        self.port = port
        self.baudrate = baudrate
    
    def send_bytes(self, bytes):
        pass
    
    def receive_bytes(self):
        pass