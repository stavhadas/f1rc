from abc import ABC, abstractmethod

class Protocol(ABC):
    def __init__(self, name):
        self.name = name
    
    @abstractmethod
    def encode(self, data):
        """ Encode data into bytes. """
        pass
        
    @abstractmethod
    def decode(self, bytes):
        """ Decode bytes into data. """
        pass