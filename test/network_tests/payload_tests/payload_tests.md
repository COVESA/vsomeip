# Payload Tests

These tests validate the vsomeip library's ability to handle different payload sizes and
communication scenarios across various transport protocols.

## Parameters

The payload tests support various configuration parameters:

**Service Parameters (`payload_test_service`):**
- `--help`: Print help information
- `--do-not-check-payload`: Skip payload data verification (use for performance measurements)

**Client Parameters (`payload_test_client`):**

*Transport Protocol:*
- `--tcp`: Send messages via TCP
- `--udp`: Send messages via UDP (default)

*Communication Mode:*
- `--sync`: Wait for acknowledge before sending next message (default)
- `--async`: Send multiple messages without waiting for service acknowledgment

*Payload Configuration:*
- `--max-payload-size`: Limit maximum payload size. Options:
  - `UDS`: Unix Domain Socket size limit (~32KB)
  - `UDP`: UDP message size limit (~1.4KB)
  - `TCP`: TCP message size limit (~4KB)
  - `<number>`: Custom size in bytes (default: UDS)

*Performance Tuning:*
- `--sliding-window-size`: Number of messages to send before waiting for acknowledgment (async mode)
- `--number-of-messages`: Number of messages to send per payload size iteration

*Service Control:*
- `--dont-shutdown-service`: Don't shutdown the service when test completes
- `--help`: Print help information


## Test Cases

### 1. local_payload_test - Basic Local Communication

Tests basic request-response communication between local service and client.

**Test Flow:**
1. Start local service offering the test service
2. Start local client that discovers and connects to the service
3. Client sends messages with increasing payload sizes
4. Service validates each message and sends acknowledgment responses

**Success Criteria:**
- All messages sent/received successfully
- No hanging TCP/UDP sockets after completion
- Both processes exit with code 0

### 2. local_payload_test_huge_payload - Large Payload Stress Test

Tests the system's ability to handle very large payloads.

**Test Flow:**
1. Start local service offering the test service
2. Start local client that discovers and connects to the service
3. Client sends several messages with a specific payload size
4. Service processes and validates each large payload

**Success Criteria:**
- Successfully sends/receives messages with a specific set payload
- Service processes all large payloads correctly

### 3. external_local_payload_test_client_external - Cross-Container Communication

Tests communication between local service and external client running in Docker container.

**Test Flow:**
1. **UDP Phase:** Start local service with `--udp`, external client tests UDP communication
2. External client waits 5 seconds for service transition
3. **TCP Phase:** Restart local service with `--tcp`, external client tests TCP communication
4. Validate both transport protocols work across container boundaries

**Success Criteria:**
- Both UDP and TCP phases complete successfully
- Cross-container communication works properly
- Service can switch between transport protocols

### 4. external_local_payload_test_client_local_and_external - Mixed Client Test

Tests simultaneous communication with both local and external clients.

**Test Flow:**
1. Start local service offering the test service
2. Start local client with `--dont-shutdown-service` flag (runs in background)
3. **UDP Phase:** External client tests UDP communication while local client is active
4. Service handles both local and external clients simultaneously
5. External client waits 5 seconds for service transition
6. **TCP Phase:** Restart service with `--tcp`, external client tests TCP communication
7. Validate multi-client scenarios

**Success Criteria:**
- Service handles multiple concurrent clients (local + external)
- Both UDP and TCP phases work with mixed client setup
- No interference between local and external clients
