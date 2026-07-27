from abc import ABC, abstractmethod
class Protocol(ABC):
    def __init__(self, name):
        self.name = name
    
    @classmethod
    @abstractmethod
    def encode(data, md):
        """ Encode data into bytes. """
        pass
        
    @abstractmethod
    def decode(self, bytes) -> tuple:
        """ Decode bytes into data. """
        pass