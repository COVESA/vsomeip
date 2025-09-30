# Subscribe notify test one

This test ensures that events are correctly received by a service consumer
subscribed to a service, when receiving either initial or normal notifications.
The service consumer sends the service provider the payloads to be notified
by the service that the service provider is offering, and then checks the
received payloads. The service has 3 events, and 2 eventgroups, and one of
the events is in both eventgroups. The goal is to check
if initial events are received correctly for all events,
namely the event that is part of both eventgroups that should
be received twice. After the initial events, the service provider will keep
notifying for all the events, and all events should only be notified once.

## Purpose

- Ensure that initial events are correctly sent/received,
namely when one event is part of multiple eventgroups
- Ensure that the following notifications, that are
not initial events, are received correctly

## Test Logic

![Diagram](docs/subscribe_notify_test.png)

### Service provider

The service provider starts by offering the test service
with the 3 events that are part of the 2 eventgroups.
The service provider will also register a message handler that will
receive the payloads that the service consumer wants to be sent by the
service provider. The service application will keep on doing
this until the service consumer sends a message to the shutdown method,
at which point the service provider application will close.

### Service consumer

The service consumer will start by waiting for the service to
be available, at which point it will send the service provider
the payload that it whishes to receive on all events.
After this, the service consumer will subscribe to the service and
its the events, and then wait to receive the notifications to be sent.
The events received after the first subscription should be the initial events,
that are sent for each eventgroups. Since one of the events is part of
both eventgroups, that event should be notified twice. After the subscription
the service consumer will set another payload to the service provider,
and it will wait to receive the new notifications, that should be
one notification for each event.
After setting and receiving 3 payloads, the service consumer will unsubscribe
to the service and repeat the whole process 2 more times, after which
the test is concluded and the service consumer sends the service provider
a message to the shutdown method and shutdowns itself.