from abc import ABC, abstractmethod
from protocols.protocol import Protocol
from queue import Queue
from threading import Thread

class Interface(ABC):
    def __init__(self, name : str, protocols : list[Protocol]):
        self.name = name
        self.protocols = protocols
        self._receive_queue = Queue()
        self._send_queue = Queue()

    @abstractmethod
    def send_bytes(self, bytes):
        """ Send bytes through the interface. """
        pass

    @abstractmethod
    def receive_bytes(self):
        """ Receive bytes from the interface. """
        pass

    def send(self, data):
        """ Send data through the interface using the first protocol. """
        if self.protocols:
           for protocol in self.protocols:
                try:
                    data = protocol.encode(data)
                    return
                except Exception as e:
                    print(f"Error encoding data with protocol {protocol.name}: {e}")
        self.send_bytes(data)