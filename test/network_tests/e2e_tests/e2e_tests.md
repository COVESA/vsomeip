# vSomeIP End-to-End (E2E) Protection

## Overview

End-to-End (E2E) protection is a safety mechanism designed to protect
safety-related data at runtime against faults on the communication link,
including:

- Random hardware faults
- Electromagnetic interference (EMC)
- Systematic faults within the vSomeIP software

The E2E protection mechanisms follow the AUTOSAR specification:
[AUTOSAR E2E Protocol](https://www.autosar.org/fileadmin/standards/R20-11/FO/AUTOSAR_PRS_E2EProtocol.pdf)

## E2E Protection Principles

### Architecture

E2E communication involves two agents:

- **Sender (Protector)**: Adds control fields (CRC, counter) to messages
  before transmission
- **Receiver (Checker)**: Validates control fields upon message
  reception

### Protection Mechanisms

E2E profiles use combinations of the following mechanisms:

| Mechanism | Description |
|-----------|-------------|
| **CRC (Cyclic Redundancy Check)** | Detects data corruption through polynomial-based checksums |
| **Sequence Counter** | Increments with each transmission; receiver validates correct incrementation |
| **Alive Counter** | Increments with each transmission; receiver checks for changes (not strict sequence) |
| **Data ID** | Unique identifier for port data elements or message groups |
| **Source ID** | Identifies the source (e.g., client) of data |
| **Message Type** | Distinguishes requests vs responses for method protection |
| **Timeout Detection** | Monitors receiver communication and sender acknowledgement timeouts |

### Profile Functionality

Each E2E profile provides three core functions:

1. **Protect**: Creates E2E header with protection metadata for transmission
2. **Forward**: Creates E2E header while allowing state forwarding
3. **Check**: Validates received E2E header and detects communication faults

## Supported E2E Profiles

### Profile 01 (CRC8)

**Mechanisms:**

| Component | Specification |
|-----------|---------------|
| **CRC** | 8-bit using CRC-8-SAE J1850 polynomial (0x1D) |
| **Counter** | 4-bit (0-14, rolls over to 0, value 15 reserved) |
| **Counter Type** | Both Alive Counter and Sequence Counter |
| **Data ID** | 16-bit, included in CRC but optionally transmitted |
| **Data ID Modes** | 0-2 (not transmitted), 3 (low nibble transmitted) |

**Header Generation:**

1. Write counter into data at specified offset
2. Write Data ID if using nibble mode (mode 3)
3. Compute CRC over Data ID and message data (excluding CRC field)
4. Write CRC into data at specified offset
5. Increment counter for next message

**Protection Capabilities:**

- Detects: repetition, loss, delay, insertion, incorrect sequence
- Detects: blocking access, masquerade, incorrect addressing
- Detects: data corruption, asymmetric information

### Profile 04 (CRC32)

**Mechanisms:**

| Component | Specification |
|-----------|---------------|
| **CRC** | 32-bit using polynomial 0xF4ACFB13 |
| **Counter** | 16-bit (0-65535) |
| **Data ID** | 32-bit, unique system-wide identifier |
| **Length** | 16-bit, supports dynamic-size data |
| **Header Placement** | Configurable offset in protected area |

**Header Generation:**

1. Verify data length within configured min/max bounds
2. Write length into data at specified offset
3. Write counter into data at specified offset
4. Write Data ID into data in Big Endian order
5. Compute CRC over data before header, header before CRC, data after header
6. Write CRC into data at specified offset
7. Increment counter for next message

**Protection Capabilities:**

- All Profile 01 protections plus:
- Detects: information received by subset of receivers
- Detects: asymmetric information from sender to multiple receivers

### Profile 07 (CRC32 with Extended Counter)

**Mechanisms:**

| Component | Specification |
|-----------|---------------|
| **CRC** | 32-bit using polynomial 0xF4ACFB13 |
| **Counter** | 32-bit (0-4294967295) |
| **Data ID** | 32-bit, unique system-wide identifier |
| **Length** | 32-bit, supports large dynamic-size data |
| **Header Placement** | Configurable offset in protected area |

**Use Case:**

Profile 07 is designed for long-running applications requiring extended
operation without counter wraparound. The 32-bit counter provides ~4.3 billion
message capacity before rollover.

### Custom Profile (CRC32 Ethernet)

**Mechanisms:**

| Component | Specification |
|-----------|---------------|
| **CRC** | 32-bit using Ethernet polynomial (0x04C11DB7) |
| **Additional Fields** | None - CRC only |
| **Header Placement** | Configurable offset |

**Header Generation:**

1. Verify data length exceeds CRC size (32 bits)
2. Compute CRC over all data except CRC field
3. Write CRC into data at specified offset

**Use Case:**

Lightweight protection using only CRC without counter mechanisms. Suitable
for scenarios where message ordering is guaranteed by lower layers.

## CRC Algorithms

The specification used to compute the CRCs in Profile 01, Profile 04,
Profile 07, and all other profiles can be consulted in the
[AUTOSAR CRC Library Specification](https://www.autosar.org/fileadmin/standards/R22-11/CP/AUTOSAR_SWS_CRCLibrary.pdf).

### CRC-8-SAE J1850 (Profile 01)

| Parameter | Value |
|-----------|-------|
| Width | 8 bits |
| Polynomial | 1Dh |
| Initial Value | FFh |
| Input Data Reflected | No |
| Result Data Reflected | No |
| XOR Value | FFh |
| Check Value | 4Bh |
| Magic Check Value | C4h |

**Implementation:** The implementation of the 8-bit CRC is done using a
lookup table. Since the polynomial is always the same, all possible CRC
values for different inputs can be precomputed. Given that the input is 8
bits wide, there are 256 possible input values, making it feasible to build
a table holding all CRC values for the given polynomial. This is the lookup
table. However, input data typically consists of multiple bytes. Therefore,
an extra step is necessary: the temporary CRC value must be XORed with the
next input byte to produce the correct lookup table index for subsequent
calculations.

### CRC-32 0xF4ACFB13 (Profile 04 & 07)

| Parameter | Value |
|-----------|-------|
| Width | 32 bits |
| Polynomial | F4ACFB13h |
| Initial Value | FFFFFFFFh |
| Input Data Reflected | Yes |
| Result Data Reflected | Yes |
| XOR Value | FFFFFFFFh |
| Check Value | 1697D06Ah |
| Magic Check Value | 904CDDBFh |
| Hamming distance | 6, up to 4096 bytes (including CRC) |

**Implementation:** The implementation of the 32-bit CRC also uses a lookup
table. However, because we are now dealing with 32 bits, the lookup table
index is given by XORing the input byte with the Most Significant Byte (MSB)
of the current CRC. The result from the lookup table is then XORed with the
current CRC shifted left by 8 bits, resulting in the final CRC after all
bytes have been processed.

### CRC-32 Ethernet (Custom Profile)

| Parameter | Value |
|-----------|-------|
| Width | 32 bits |
| Polynomial | 04C11DB7h |
| Initial Value | FFFFFFFFh |
| Input Data Reflected | Yes |
| Result Data Reflected | Yes |
| XOR Value | FFFFFFFFh |
| Check Value | CBF43926h |
| Magic Check Value | DEBB20E3h |

**Implementation:** Same lookup table approach as Profile 04/07 CRC,
different polynomial generates different table.

## Configuration

### JSON Configuration Example (Profile 01)

**Service Side (Protector):**

```json
{
  "e2e": {
    "e2e_enabled": "true",
    "protected": [
      {
        "service_id": "0x1234",
        "event_id": "0x8001",
        "profile": "CRC8",
        "variant": "protector",
        "crc_offset": "0",
        "data_id_mode": "3",
        "data_length": "24",
        "data_id": "0x1FF"
      }
    ]
  }
}
```

**Client Side (Checker):**

```json
{
  "e2e": {
    "e2e_enabled": "true",
    "protected": [
      {
        "service_id": "0x1234",
        "event_id": "0x8001",
        "profile": "CRC8",
        "variant": "checker",
        "crc_offset": "0",
        "data_id_mode": "3",
        "data_length": "24",
        "data_id": "0x1FF"
      }
    ]
  }
}
```

**Key Differences:**

- Service uses `variant: "protector"`
- Client uses `variant: "checker"`
- All other parameters must match exactly

## Test Cases

### 1. e2e_test_external - Basic E2E Protection

Tests fundamental E2E protection using Profile 01 (CRC8) and Custom Profile
(CRC32 Ethernet).

**Test Flow:**

1. **Master Side:** Start external client with remote configuration
2. **Slave Side:** Start external service with remote configuration
3. **Profile 01 Testing:**
   - Client sends requests with CRC 8-bit protection
   - Service validates CRC and counter, sends protected responses
   - Client subscribes to events with E2E protection
4. **Custom Profile Testing:**
   - Client sends requests with CRC32 protection
   - Service validates enhanced protection, sends responses
   - Client subscribes to custom profile events

**E2E Validation:**

- CRC calculation and verification for all messages
- Counter increment validation (4-bit for Profile 01)
- Event notification integrity checking
- Response payload CRC validation

### 2. e2e_profile_04_test_external - High-Integrity Protection

Tests E2E Profile 04 with enhanced CRC32 and 16-bit counter protection.

**Test Flow:**

1. **Master Side:** Start Profile 04 client in remote mode
2. **Slave Side:** Start Profile 04 service in remote mode
3. **Enhanced Protection Testing:**
   - Client sends requests with CRC32 protection
   - Service validates 32-bit CRC and 16-bit counter
   - Client subscribes to events with Profile 04 protection
   - Service sends events with enhanced integrity protection

**Profile 04 Features:**

- **CRC32 Algorithm**: Stronger error detection capability
- **16-bit Counter**: Extended counter range (0-65535)
- **Fixed Payload Validation**: Known test vectors for CRC verification
- **Event Protection**: Events include full E2E protection headers

### 3. e2e_profile_07_test_external - Maximum Protection

Tests E2E Profile 07 with CRC32 and 32-bit counter for long-running applications.

**Test Flow:**

1. **Master Side:** Start Profile 07 client in remote mode
2. **Slave Side:** Start Profile 07 service in remote mode
3. **Maximum Protection Testing:**
   - Client sends requests with full protection
   - Service validates CRC32 and 32-bit counter
   - Client subscribes to events with Profile 07 protection
   - Service maintains extended counter ranges

**Profile 07 Features:**

- **CRC32 Algorithm**: Maximum error detection capability
- **32-bit Counter**: Extended operation without wraparound
- **Long-term Reliability**: Suitable for continuous operation
- **Extended Headers**: Full protection metadata

## Message Flow Patterns

### Request-Response Pattern

```text
Client -> Service: Unprotected Request
Service: Apply E2E Protection (Protect function)
Service -> Client: Protected Response (CRC + Counter + Data ID)
Client: Validate E2E Header (Check function)
```

### Event Notification Pattern

```text
Client -> Service: Subscribe to Protected Event
Service: Apply E2E Protection to Event Data
Service -> Client: Protected Event Notification (CRC + Counter)
Client: Validate E2E Header (Check function)
```

### Shutdown Pattern

```text
Client -> Service: Shutdown Request (Method 0x0002/0x7777)
Service: Stop offering service and exit
```

## Fault Detection Capabilities

E2E protection profiles can detect the following communication faults:

| Fault Type | Description | Detected By |
|------------|-------------|-------------|
| **Repetition** | Same message received multiple times | Counter |
| **Loss** | Messages missing from sequence | Counter |
| **Delay** | Messages received out-of-order | Counter |
| **Insertion** | Unauthorized messages injected | CRC |
| **Masquerade** | Impersonation of legitimate sender | Data ID |
| **Corruption** | Bit errors in message payload | CRC |
| **Incorrect Sequence** | Messages in wrong order | Counter |
| **Incorrect Addressing** | Message sent to wrong destination | Data ID |
| **Asymmetric Info** | Different data received by different receivers | CRC + Data ID |
| **Blocking Access** | Communication channel blocked | Timeout + Counter |

## References

- [AUTOSAR E2E Protocol Specification](https://www.autosar.org/fileadmin/standards/R20-11/FO/AUTOSAR_PRS_E2EProtocol.pdf)
- [AUTOSAR CRC Library Specification](https://www.autosar.org/fileadmin/standards/R22-11/CP/AUTOSAR_SWS_CRCLibrary.pdf)
