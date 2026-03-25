# Npdu Test

This test assures that all messages sent within the set debounce time receive a response from service provider.

The test also doubles the payload size with each iteration until it reaches the max payload size (VSOMEIP_MAX_LOCAL_MESSAGE_SIZE)

## Purpose

- Assure that debounce times are respected with increasingly bigger payloads
- Assure that debounce times are respected both in TCP and UDP communication

## Test Logic

This test is intended to test the functionality of the so called nPDU feature. The test setup is as followed:

* There are two nodes, one hosting the services, one hosting the clients.
* On each of the nodes is a routing manager daemon (RMD) started whose only
  purpose is to provide routing manager functionality and shutdown the clients
  and services at the end.
* There are four services created. Each of the services has four methods.
* All services are listening on the same port. Therefore there only is:
    * one server endpoint created in the RMD on service side
    * one client endpoint created in the RMD on client side
* There are four clients created. Each of the clients will:
    * Create a thread for each service
    * Create a thread for each method of each service
    * Send multiple messages with increasing payload to each of the services'
      methods from the corresponding thread.
    * After sending the threads will sleep the correct amount of time to insure
      applicative debounce > debounce time + max retention time.
* After all messages have been sent to the services the clients will notify the
  RMD that they're finished. The RMD then instructs the RMD on service side to
  shutdown the services and exit afterwards. After that the RMD on client side
  exits as well.
* Upon receiving a method call the service will check if the debounce time
  specified in the json file for this method was undershot and print out a
  warning.
* The test first runs in synchronous mode and waits for a response of the
  service before sending the next message.
