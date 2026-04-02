from .interface import Interface
from socket import socket, SHUT_RDWR

class SocketInterface(Interface):
    def __init__(self, name, ip, port, protocols):
        super().__init__(name, protocols)
        self.ip = ip
        self.port = port
        self.socket = socket()
    
    def start(self):
        self.socket.connect((self.ip, self.port))
        super().start()
    
    def send_bytes(self, bytes):
        """ Send bytes through the interface. """
        self.socket.send(bytes)

    def receive_bytes(self):
        """ Receive bytes from the interface. """
        return []
    
    def stop(self):
        self.socket.shutdown(SHUT_RDWR)
        self.socket.close()
        super().stop()