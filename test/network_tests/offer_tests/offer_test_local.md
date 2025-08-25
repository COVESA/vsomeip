# Offer Test Local

The test is started by the **offer_test_local_starter.sh** script. which contains 5 test cases:

1. Rejecting offer of service instance whose hosting application is still alive
2. Rejecting offer of service instance whose hosting application is still alive with daemon
3. Accepting offer of service instance whose hosting application crashed (with send SIGKILL)
4. Accepting offer of service instance whose hosting application became unresponsive (SIGSTOP)
5. Rejecting offers for which there is already a pending offer

## Test Logic

### 1st Test Case - Rejecting offer of service instance whose hosting application is still alive

1. Start application which offers service (This application will be the routing HOST)
2. Start two clients which continuously exchanges messages with the service
3. Start application which offers the same service again
   - Should be rejected and an error message should be printed
4. Message exchange with client application should not be interrupted

### 2nd Test Case - Rejecting offer of service instance whose hosting application is still alive with daemon

1. Start routingmanagerd as the HOST (needed as he has to ping the offering client)
2. Start application which offers service
3. Start two clients which continuously exchanges messages with the service
4. Start application which offers the same service again
   - Should be rejected and an error message should be printed
5. Message exchange with client application should not be interrupted

### 3rd Test Case - Accepting offer of service instance whose hosting application crashed (with send SIGKILL)

1. Start routingmanagerd as the HOST
2. Start application which offers service
3. Start client which exchanges messages with the service
4. Kill application with SIGKILL
5. Start application which offers the same service again
   - Should be accepted
6. Start another client which exchanges messages with the service
   - Client should now communicate with new offerer

### 4th Test Case - Accepting offer of service instance whose hosting application became unresponsive (SIGSTOP)

1. Start routingmanagerd as the HOST
2. Start application which offers service
3. Send a SIGSTOP to the service to make it unresponsive
4. Start application which offers the same service again
   - Should be marked as PENDING_OFFER and a ping should be sent to the paused application
   - After the timeout passed the new offer should be accepted
5. Start client which exchanges messages with the service
   - Client should now communicate with new offerer

### 5th Test Case - Rejecting offers for which there is already a pending offer

1. Start routingmanagerd as the HOST
2. Start application which offers service
3. Send a SIGSTOP to the service to make it unresponsive
4. Start application which offers the same service again
   - Should be marked as PENDING_OFFER and a ping should be sent to the paused application
5. Start application which offers the same service again
   - Should be rejected as there is already a PENDING_OFFER pending
   - After the timeout passed the new offer should be accepted
6. Start client which exchanges messages with the service
   - Client should now communicate with new offerer.
