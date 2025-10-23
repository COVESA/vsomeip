# Initial Event Test

This test checks whether vsomeip correctly sends initial events to other applications on
subscription to an event group that contains fields.

## Parameters

This test contains multiple parameters to test the initial event mechanics under different
scenarios. These include:

- the protocol used for communication (UDP, TCP or both)
- the ports used for communication
- the number of events that are offered

## Test Logic

The test setup involves two containers, each with 3 services and 10 clients.

1. The service applications on each container start offering the test service.

2. The test waits for the other container to also start its services.

3. The client applications subscribe to each service and wait to receive notifications.

4. The test waits for all clients to exit and then terminates the services.
