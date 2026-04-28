# Header Factory Tests

This test assures that request and response messages are created with the correct header fields and that the header fields of a request and its response match. It also assures that the client id and session id fields are correctly set when sending and receiving messages.

## Purpose

- Assure that request and response messages are created with the correct header fields
- Assure that the header fields of a request and its response match
- Assure that the client id and session id fields are correctly set when sending and receiving messages

## Test Logic

The following things are tested:
- create request
    - check  "Protocol Version" / "Message Type" / "Return Type" fields
- create request, fill header, create response
    - compare header fields of request & response
- create notification
    - check  "Protocol Version" / "Message Type" / "Return Type" fields
- create message, fill header (service/instance/method/interface version/message type)
    - send message 10 times
    - receive message and check client id / session id
