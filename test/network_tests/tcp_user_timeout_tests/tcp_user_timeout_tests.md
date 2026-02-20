# tcp user timeout tests

This test ensures that the TCP_USER_TIMEOUT definition for the tcp sockets
is correctly defined and in effect.

This is done by having the routing host and a client communicate via
local tcp and analyzing their behavior when there is a break in communications
while the routing host has packets to be sent to the client.

The expected behavior is that the TCP_USER_TIMEOUT is able to detect
the break in communications, drop the connection without going into
a TIME_WAIT state when there is data to be sent,
and after the communications resume, the client should be able to
reestablish its connection to the routing host.

Previously, if there was data to be sent from the routing host to
the client when there was a break in communications,
the connection would try to retransmit the data for a long
period of time, delaying the creation of a new
connection after the communication resume.

## Purpose

- Verify that the TCP_USER_TIMEOUT socket option is enabled
and working as intended.

- Verify that the TCP connections between the routing host
and a client are destroyed if communications stops
when there is data to be sent to the client, and when this happens,
connections are not stuck on retransmission after the resume
in communications leading to a stall on the client side.

- Verify that after the resume of communications,
the client is able to register to the routing host again.

## Test Logic

### Test script

In this test, the test scripts play a fundamental role on its behavior.

Is only through the script that the network state
can be monitored and its behavior can be changed.

After launching the service and client applications,
where the service application will act as routing host,
the script begins the monitoring of the tcp socket queue size
of the connection between service and client,
so that communications can be stopped while the
service application endpoint socket queue has data
to be sent to the client.

Meanwhile, the client will track its registration status. What is expected
to happen is that when the client detects that it has deregistered due to the
timeout, it sends a signal to the script by creating a file, and as such
it will signal the test script that it has detected that its connection to
the service has timed out.

The script, after stopping the communications, will continuously
poll for the existence of the file created by the client, and when
this is detected the script will know that the service client connection
has timed out and will reenable the communications.

After this, it is expected that the client registers again,
after which it will finish its execution, and will signal the service
by creating a file that it can finish its execution.

### Service application

The service application is the routing host and uses local tcp in order to
establish the connections to the client.

Since it is using local tcp, if the tcp communications are stopped
for the duration of TCP_USER_TIMEOUT(3 seconds),
its connection to the client will be deleted in order to establish a new one
immediately after the communications resume,
without the previous connection between them being stuck in retransmission,
deregistering the client when it detects the timeout and re-registering
when the communications resume.

In order for the communications to stop while there is data
to be sent to the client, the service application will register and offer
the test service and will continuously send a huge amount of notifications,
until the script signals that its socket queue has been filled by creating a file.
The goal of sending the notification in a high rate is to overwhelm the
connection between the service and the client.

The server will continue existing, even when not sending notifications,
since it acts as the routing host and it is neccessary that it exists
for the client to re-register. The service will continue its execution
until it detects that the client has created the shutdown file and
it can conclude its execution.

### Client application

The client application will communicate with the service via local tcp.

The client will request and subscribe to the test service,
from which it will continuously receive notifications.

Meanwhile, it will track its registration status in
order to detect if its connection to the service has been timed out.

It is expected that the client first correctly registers to the routing
manager, and when communications stop, its connection to the
service breaks and the client is deregistered. After the communications are restored
it is expected that the client re-registers after which it will create
the shutdown file signaling the service that it can conclude its execution
and will shutdown itself aswell.
