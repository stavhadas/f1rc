from abc import ABC, abstractmethod
from protocols.protocol import Protocol
from queue import Queue
from threading import Thread, Event

class Packet:
    def __init__(self):
        self.payload = []
        self.md = {}
class Interface(ABC):
    def __init__(self, name : str, protocols : list[Protocol]):
        self.name = name
        self.protocols = protocols
        self._receive_queue = Queue()
        self._send_queue = Queue()
        self._send_thread = Thread(target=self.send_thread_entry, daemon = True)
        self._recv_thread = Thread(target=self.recv_thread_entry, daemon = True)
        self._stop_event = Event()

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
        self._send_queue.put(data)
        
    
    def send_thread_entry(self):
        while self._stop_event.is_set():
            packet = self._send_queue.get()
            if self.protocols:
                for protocol in self.protocols:
                    try:
                        packet.payload = protocol.encode(packet.payload, packet.md)
                    except Exception as e:
                        print(f"Error encoding data with protocol {protocol.name}: {e}")
                payload = packet.payload
            else:
                payload = packet.payload
            self.send_bytes(payload)
    
    def recv_thread_entry(self):
        while self._stop_event.is_set():
            data = self.receive_bytes()
            if data:
                for protocol in self.protocols[::-1]:
                    try:
                        data, md = protocol.decode(data)
                        return
                    except Exception as e:
                        print(f"Error encoding data with protocol {protocol.name}: {e}")
    def start(self):
        self._stop_event.clear()
        self._recv_thread.start()
        self._send_thread.start()
        
    def stop(self):
        self._stop_event.set()
        self._recv_thread.join()
        self._send_thread.join()
        