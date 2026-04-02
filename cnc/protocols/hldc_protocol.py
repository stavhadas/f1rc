from protocols.protocol import Protocol

START_FLAG  = 0x7E
ESCAPE_CHAR = 0x7D
ESCAPE_XOR  = 0x20

class HLDCProtocol(Protocol):
    name = "HLDC"

    @staticmethod
    def _crc16(data: bytes) -> int:
        crc = 0xFFFF
        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
        return crc

    @staticmethod
    def _byte_stuff(data: bytes) -> bytes:
        out = bytearray()
        for byte in data:
            if byte == START_FLAG or byte == ESCAPE_CHAR:
                out.append(ESCAPE_CHAR)
                out.append(byte ^ ESCAPE_XOR)
            else:
                out.append(byte)
        return bytes(out)

    @classmethod
    def encode(cls, data: bytes, md: dict) -> bytes:
        address = md.get('address', 0)
        control = md.get('control', 0)

        crc = cls._crc16(bytes([address, control]) + data)
        crc_bytes = bytes([crc & 0xFF, (crc >> 8) & 0xFF])

        return (
            bytes([START_FLAG, address, control])
            + cls._byte_stuff(data)
            + cls._byte_stuff(crc_bytes)
            + bytes([START_FLAG])
        )

    @classmethod
    def decode(cls, data: bytes) -> tuple:
        i = 0
        while i < len(data):
            if data[i] != START_FLAG:
                i += 1
                continue

            j = i + 1
            if j + 2 > len(data):
                break

            address = data[j];     j += 1
            control = data[j];     j += 1

            # Un-byte-stuff until end flag
            payload = bytearray()
            found_end = False
            while j < len(data):
                byte = data[j];  j += 1
                if byte == START_FLAG:
                    found_end = True
                    break
                elif byte == ESCAPE_CHAR:
                    if j >= len(data):
                        break
                    payload.append(data[j] ^ ESCAPE_XOR);  j += 1
                else:
                    payload.append(byte)

            if not found_end or len(payload) < 2:
                i += 1
                continue

            received_crc = payload[-2] | (payload[-1] << 8)
            actual_payload = bytes(payload[:-2])

            if cls._crc16(bytes([address, control]) + actual_payload) != received_crc:
                i += 1
                continue

            return actual_payload, {'address': address, 'control': control}

        raise ValueError("No valid HLDC frame found in data")
