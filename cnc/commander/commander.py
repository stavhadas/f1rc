
import hashlib
import types
from pathlib import Path
import importlib.util
from interfaces.interface import Interface, Packet
from grpc_tools import protoc

def import_from_path(module_name: str, path: str):
    spec   = importlib.util.spec_from_file_location(module_name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

CACHE_FILE_NAME = "commander_cache.txt"




class Commander:
    def make_send_command(self, cmd_name, cmd_struct):
        def send_command(self, **kwargs):
            print(f"Sending {cmd_name.lower()}")
            msg    = self.protobuf.Command()
            fields = cmd_struct.message_type.fields

            if fields:
                payload = getattr(msg, cmd_name)
                for field in fields:
                    if field.name not in kwargs:
                        print(f"Missing {field.name}")
                        return 0
                    setattr(payload, field.name, kwargs.get(field.name))
            else:
                getattr(msg, cmd_name).SetInParent()
            packet = Packet()
            packet.payload = msg.SerializeToString() 
            self.interface.send(packet)
            
            should_wait = kwargs.get("should_wait", False)
            response = self.protobuf.Response().DESCRIPTOR.fields_by_name.get(cmd_name, None)
            if should_wait and response:
                print("Waiting for response")
            return 1
        return send_command
    def __init__(self, interface : Interface, proto_path : str, proto_file : str, cache_dir : str):
        self.interface = interface
        self.cache_dir = cache_dir
        new_hash = self._hash_file(Path(proto_path) / Path(proto_file))
        self.hash_file = Path(self.cache_dir, CACHE_FILE_NAME).resolve()
        if not Path.exists(self.hash_file):
            Path(self.hash_file).touch()
        if new_hash != self._get_cached_hash():
            print("Protocol file has changed, updating cache.")
            with open(self.hash_file, "w") as f:
                f.write(new_hash)
            print(self._get_cached_hash())
            self.generate_protobuf(proto_file, proto_path, self.cache_dir)
        else:
            print("Protocol file has not changed, using cached version.")
        self.protobuf = import_from_path("main_pb2" , Path(self.cache_dir)/Path("main_pb2.py"))
        for command_name in self.protobuf.Command().DESCRIPTOR.fields_by_name:
            command_struct = self.protobuf.Command().DESCRIPTOR.fields_by_name[command_name]
            setattr(self, f"send_{command_name.lower()}", types.MethodType(self.make_send_command(command_name, command_struct), self))



    def generate_protobuf(self, proto_file: str, proto_path: str, output_dir: str) -> None:
        result = protoc.main([
        "grpc_tools.protoc",
        f"--proto_path={str(proto_path)}",
        f"--python_out={str(output_dir)}",
        str(proto_file),
        ])

        if result != 0:
            raise RuntimeError(f"protoc failed on {proto_file}")

            print(f"Generated protobuf files in {output_dir}")



    def _hash_file(self, path: str, algorithm: str ="sha256") -> str:
        h = hashlib.new(algorithm)

        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(8192), b""):  # read in 8KB chunks
                h.update(chunk)                             # handles large files

            return h.hexdigest()

    def _get_cached_hash(self):
        hash = ""
        with open(self.hash_file, "r") as f:
            hash = f.read()
        return hash
