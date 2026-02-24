# Invalid Event Test

## Purpose

This test checks if the server continues to acknowledge valid subscriptions after a client tries to
register an invalid event with the wrong reliability type.

## Flow

```mermaid
sequenceDiagram
    box ECU
        participant Server
        participant Client
        participant Router
    end
    participant Remote

    Server->>Router: offer_event{id=1,reliable=1}
    Server->>Router: OFFER

    Remote->>Router: FIND
    Router->>Remote: OFFER
    Remote->>Router: SUBSCRIBE
    Router->>Remote: SUBSCRIBE ACK

    Client->>Router: request_event{id=2, reliable=2}
    Client->>Router: REQUEST

    alt Good Case
        Remote->>Router: SUBSCRIBE
        Router->>Remote: SUBSCRIBE ACK
    else Bad Case
        Remote->>Router: SUBSCRIBE
        Router->>Remote: SUBSCRIBE NACK
    end
```

The test flows like this:

- The Server offers the test service and registers one event as reliable.
- The Remote successfully subscribes to that event.
- The Client registers a different event with the wrong reliability.
- The Remote **should** still be able to subscribe to the original event.
