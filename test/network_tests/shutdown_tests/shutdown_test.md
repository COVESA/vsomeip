# Shutdown Test

These tests ensure that message delivery reaches its destination in the case where an application sends a message and immediately stops.

## Purpose

- Verify that messages are properly delivered even when the sender application terminates immediately after sending
- Ensure that the routing manager correctly handles rapid shutdown scenarios
- Validate that message queues are flushed before application termination

## Test Logic

Shutdown_test has 2 test cases:

1. Client sends 5 shutdown messages and stops immediately
2. Service sends 5 replies to shutdown message and stops immediately

### 1st Test Case - Client sends 5 shutdown messages and stops immediately

1. Start the applications and Service offers
2. The Client subscribes, sends 5 shutdown messages and stops immediately - The 5 messages must reach its destination
3. The Service receives the 5 shutdown messages and stops

### 2nd Test Case - Service sends 5 shutdown replies and stops immediately

1. Start the applications and Service offers
2. The Client subscribes and sends a shutdown message
3. The Service receives the shutdown message, sends 5 replies and stops immediately - The 5 messages must reach its destination
4. The Client receives the 5 shutdown replies and stops

## Executions

### Different scenarios for local communication covered in TCP and UDS

1. Routingmanagerd (Host), Client and Service
2. Client (Host) and Service
3. Service (Host) and Client
