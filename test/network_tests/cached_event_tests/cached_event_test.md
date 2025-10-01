# Cached Event Test

## Purpose

Check whether field notifications are correctly cached for new subscribers.

## Setup

The test setup involves two containers:

- Container A has 2 clients and a routing manager.
- Container B has 1 service provider.

## Logic

1. Client A1 starts and requests service B to the routing manager.

2. Service B starts and offers the test service.

3. Client A1 subscribes to the test service and receives the initial event.

4. Client checks that the notification is correct.

5. Client A2 starts and subscribes to service B.

6. Client A2 receives the cached initial event from the routing manager.

7. Client A2 checks that the notification is correct.

*Note: Although it's not required for the client to request the service before it is offered, in
practice this helps cover an edge-case in the cache implementation.*
