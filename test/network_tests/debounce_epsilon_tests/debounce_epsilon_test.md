# Debounce Epsilon Test

This test checks whether an epsilon change function is correctly used to debounce outgoing events.

The purpose of the epsilon change function is to allow the service provider to control when an event
should be sent to subscribed clients by comparing it the last sent payload.

This is useful, for example, when you have a frequently updated attribute, such as current position,
which would generate a lot of traffic if an event was sent on every change.

Instead, an epsilon change function can be used to only update the position when the car has moved
a sufficient distance from the last position.

Requirements: PRS_SOMEIP_00183.

## Test Logic

1. The test client is started and requests the test service.

2. Test service is started and offered. The service provider defines an epsilon change function
   to only allow events where value of the first byte of the payload is divisible by 10.

3. The service provider starts trying to send notifications with incrementally larger payload
   values.

4. Client subscribes to test service.

5. Client receives notifications and checks if the payload is divisible by 10, indicating that the
   debounce is working.

6. Client signals service provider to stop.

7. Service provider confirms stop command and shuts down.

8. Client receives the stop confirmation and shuts down.
