# Connection timeout sub ack Test

Check if after a client endpoint detects a timeout, after communication is
restored the client receives a subscribe ack after subscribing to the service.

## Purpose

Before, a client would not receive a subscribe ack nor trigger the subscription
status handler when subscribing to a service after detecting a connection
timeout and resuming the connection to the service. This test aims to check
that the client correctly receives the subscribe ack and the subscription
status handler is triggered after recovering from a connection timeout.

## Test Logic

In this test, client is run as routing host on the master container, while
service is run as routing host on the slave container. The master script will,
depending on signals sent via creating a file by the client, control the
communications between containers, first breaking communications between
containers in order to trigger a timeout on the client and then restoring
the communications in order for the client to subscribe again to the service
and check if the client has its subscription status handler triggered.

```text
Start script

Start client

Start service

Client requests/subscribes to the service

Service offers the service

Client receives subscribe ack and signals to script

Script breaks communications between containers

Client detects timeout, receives service unavailability and signals

Script restores communications between containers and waits for apps finish

Client receives second subscribe, signals shutdown

Service receives shutdown and finishes
```
