import inspect
from pathlib import Path

from interfaces.serial_interface import SerialInterface
from interfaces.socket_interface import SocketInterface
from protocols.hldc_protocol import HLDCProtocol
from commander.commander import Commander

BASE_DIR  = Path(inspect.getfile(inspect.currentframe())).parent.resolve()

class ControllerCNC:
    def __init__(self):
        commander_cache_dir_path = Path(BASE_DIR, "..", "build/python-generated/controller/").resolve()
        Path(commander_cache_dir_path).mkdir(parents=True, exist_ok=True)
        # self.command_interface = SerialInterface("Command Interface", [], "/dev/ttyUSB0", 115200)
        self.command_interface = SocketInterface("socket", "127.0.0.1", 1337, [HLDCProtocol])
        self.commander = Commander(self.command_interface, Path(BASE_DIR, '..', "apps/controller/protobuf/").resolve(), "main.proto", commander_cache_dir_path)

    def start(self):
        self.command_interface.start()

    def stop(self):
        self.command_interface.stop()