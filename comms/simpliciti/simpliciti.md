# Custom SimpliciTI-Compliant P2P Stack
**Target Hardware:** STM32 Blackpill (MCU) & TI CC1101 (Transceiver)
**Topology:** Direct Peer-to-Peer (End Device to End Device)
**Duplexing:** Time Division Duplexing (TDD)

## 1. System Overview and Scope
This document outlines the requirement specifications for a bare-metal, lightweight Layer 2 (MRFI) and Layer 3 (NWK) RF communication stack. It is strictly based on the SimpliciTI protocol, stripped down for a pure direct P2P topology. 

* **No Infrastructure:** No Access Points (AP) or Range Extenders (RE) are supported.
* **Security:** Disabled for the initial implementation.
* **Frequency Agility:** Fixed frequency operation.
* **Endianness:** All multi-byte network objects must be transmitted in little-endian format.

---

## 2. Layer 2: Minimal RF Interface (MRFI) Requirements
The MRFI abstracts the CC1101 SPI interface and manages medium access and frame encapsulation.

### 2.1 Frame Format & Hardware Delegation
The CC1101 hardware is responsible for inserting the `PREAMBLE`, `SYNC` words, and appending the `FCS` (CRC). The MRFI software handles the MAC payload (maximum 64 bytes total FIFO size).

**MRFI TX/RX Payload Structure:**
1.  **Length (1 byte):** Length of the remaining packet (inserted on TX, used by CC1101 hardware).
2.  **DstAddr (4 bytes):** Destination Address.
3.  **SrcAddr (4 bytes):** Source Address.
4.  **NWK Payload (n bytes):** Passed to/from L3.

### 2.2 Addressing & Filtering
* **Local Address:** Devices must be provisioned with a unique 4-byte address.
* **Filtering:** The MRFI must discard frames where `DstAddr` does not match the local address or the network broadcast address (`0xFFFFFFFF`). This can be implemented via CC1101 hardware filtering or MRFI software.

### 2.3 Medium Access
* **Listen-Before-Talk (LBT):** Before asserting TX, the MRFI must read the CC1101 Clear Channel Assessment (CCA) pin/register to ensure the channel is clear, compliant with ETSI specifications.

---

## 3. Layer 3: Network Layer (NWK) Requirements
The NWK layer manages connections, frame routing to internal ports, and transaction state.

### 3.1 NWK Header Structure
Encapsulated within the MRFI payload, the NWK header precedes all application data.

1.  **PORT (1 byte):**
    * `Bits 5-0`: Logical port number.
    * `0x00 - 0x1F`: Reserved for NWK management.
    * `0x20 - 0x3E`: User application ports.
    * `0x3F`: Broadcast port.
    * `Bit 6`: Encryption status (`0` = plaintext).
    * `Bit 7`: Forwarded frame (`0` for direct P2P).
2.  **DEVICE INFO (1 byte):**
    * `Bit 7`: ACK Request (`1` = ACK required).
    * `Bit 6`: Rx Type (`00` = Controlled listen, `01` = Polling/Sleep).
    * `Bits 5-4`: Sender Type (Must be `00` for End Device).
    * `Bit 3`: ACK Reply (`1` = Frame is an ACK response).
    * `Bits 2-0`: Hop Count (Initialize to max, ignore on RX in P2P).
3.  **TRACTID (1 byte):** Transaction ID for duplicate rejection and reply matching.

### 3.2 TRACTID Discipline
* **Initiator:** Generates a new `TRACTID` (sequential or otherwise) for every *new* transaction.
* **Responder:** Must *echo* the exact `TRACTID` received in the request back to the initiator in the reply frame.

---

## 4. Connection Management & State Machines (Management Ports)

### 4.1 Port 0x01: Ping (Presence Detection)
Ping is unicast and blocking on the initiator side — the initiator waits for a reply with a timeout applied.

**NWK Header:** Port byte is `0x01` for both request and reply (`forwarded=0`, `encrypted=0`, `app_port=1`). The `DEVICE_INFO` byte carries standard sender/rx-type fields and is not used to distinguish request from reply.

**Payload Structure:**

| Byte | Field | Request | Reply |
|---|---|---|---|
| 0 | App Info | `0x01` | `0x80` (MSB set = reply) |

* The **App Info MSB** (`0x80`) is the sole indicator that a frame is a ping reply. The initiator uses this to distinguish a reply from a new incoming request on the same port.
* The **NWK header `TRACTID`** is the sole transaction identifier — there is no duplicate TID in the payload.
* **Initiator:** Sends request with `app_info=0x01`. Enters `WAIT_FOR_REPLY` with timeout.
* **Responder:** On receipt, sends unicast reply with `app_info=0x80`. The NWK header `TRACTID` is echoed per standard TRACTID discipline (section 3.2).

### 4.2 Port 0x02: Link Establishment
Requires a bi-directional handshake to populate the local Connection Table.
* **Client (Initiator):** 1. Broadcasts Link Request: `0x01` | `TRACTID` | `4-byte Link Token` | `Local Port` | `Rx Type` | `Protocol Ver`.
    2. Starts timeout timer.
    3. On RX of valid Link Reply, stores Server Address & Port in Connection Table. Returns `Link ID`.
* **Server (Responder):**
    1. Blocks in Listen state.
    2. Validates incoming Link Token.
    3. Sends Unicast Link Reply: `0x81` | `TRACTID` | `Local Port` | `Rx Type`.
    4. Stores Client Address & Port in Connection Table. Returns `Link ID`.

### 4.3 Port 0x02: Unlink (Teardown)
Connections are persistent. Dead links must be manually torn down.
* **Originator:** Disables local Connection Table entry. Sends Unicast Unlink Request: `0x02` | `TRACTID` | `Remote Port`.
* **Peer:** Uses `Remote Port` and `SrcAddr` to locate and destroy its Connection Table entry. Replies with Unicast: `0x82` | `TRACTID` | `Result Code`.

---

## 5. Timeout, Retry, and Resilience Architecture
The L3 state machine must handle the inherently lossy nature of RF.

### 5.1 Initiator Timeout Handling
* L3 maintains a **Pending Transaction Table** tracking: `Expected_TRACTID`, `Expected_Command`, `Peer_Address`, and `Timeout_Timestamp`.
* If a frame arrives matching these parameters, the transaction succeeds.
* If the hardware timer exceeds `Timeout_Timestamp`, the pending state is cleared, and `TIMEOUT` is returned to the application.

### 5.2 Application Retries & TRACTID Reuse
* Retries are driven by the Application Layer, not NWK.
* If the application retries a timed-out operation, the NWK layer **MUST reuse the exact same `TRACTID`** used in the failed attempt.

### 5.3 Server Duplicate Rejection (Idempotence)
* The Server L3 must maintain a cache of recently processed `[SrcAddr + TRACTID]` pairs.
* If a frame arrives matching a cached pair, the Server **must not** pass the payload to the application layer (prevents duplicate execution).
* The Server **must** immediately re-transmit the NWK reply (e.g., the ACK or Ping Reply) echoing that `TRACTID`, assuming the original reply was lost in the RF medium.