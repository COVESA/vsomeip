Configuration Test
------------------
To start the configuration test from the build directory do:

./configuration-test -someip ../config/vsomeip-test.json

The expected output is:

2015-02-10 08:47:31.503874 [info] Test "HOST ADDRESS" succeeded.
2015-02-10 08:47:31.507609 [info] Test "HAS CONSOLE" succeeded.
2015-02-10 08:47:31.507865 [info] Test "HAS FILE" succeeded.
2015-02-10 08:47:31.508001 [info] Test "HAS DLT" succeeded.
2015-02-10 08:47:31.508143 [info] Test "LOGFILE" succeeded.
2015-02-10 08:47:31.508315 [info] Test "LOGLEVEL" succeeded.
2015-02-10 08:47:31.508456 [info] Test "RELIABLE_TEST_1234_0022" succeeded.
2015-02-10 08:47:31.508593 [info] Test "UNRELIABLE_TEST_1234_0022" succeeded.
2015-02-10 08:47:31.508759 [info] Test "RELIABLE_TEST_1234_0023" succeeded.
2015-02-10 08:47:31.508896 [info] Test "UNRELIABLE_TEST_1234_0023" succeeded.
2015-02-10 08:47:31.509032 [info] Test "RELIABLE_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.509185 [info] Test "UNRELIABLE_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.509330 [info] Test "RELIABLE_TEST_4466_0321" succeeded.
2015-02-10 08:47:31.509467 [info] Test "UNRELIABLE_TEST_4466_0321" succeeded.
2015-02-10 08:47:31.509602 [info] Test "RELIABLE_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.509771 [info] Test "UNRELIABLE_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.509915 [info] Test "ADDRESS_TEST_1234_0022" succeeded.
2015-02-10 08:47:31.510049 [info] Test "MIN_INITIAL_DELAY_TEST_1234_0022" succeeded.
2015-02-10 08:47:31.510354 [info] Test "MAX_INITIAL_DELAY_TEST_1234_0022" succeeded.
2015-02-10 08:47:31.510610 [info] Test "REPETITION_BASE_DELAY_TEST_1234_0022" succeeded.
2015-02-10 08:47:31.513978 [info] Test "REPETITION_MAX_TEST_1234_0022" succeeded.
2015-02-10 08:47:31.514177 [info] Test "CYCLIC_OFFER_DELAY_TEST_1234_0022" succeeded.
2015-02-10 08:47:31.514280 [info] Test "CYCLIC_REQUEST_DELAY_TEST_1234_0022" succeeded.
2015-02-10 08:47:31.514397 [info] Test "MIN_INITIAL_DELAY_TEST_1234_0023" succeeded.
2015-02-10 08:47:31.514618 [info] Test "MAX_INITIAL_DELAY_TEST_1234_0023" succeeded.
2015-02-10 08:47:31.514754 [info] Test "REPETITION_BASE_DELAY_TEST_1234_0023" succeeded.
2015-02-10 08:47:31.514901 [info] Test "REPETITION_MAX_TEST_1234_0023" succeeded.
2015-02-10 08:47:31.515052 [info] Test "CYCLIC_OFFER_DELAY_TEST_1234_0023" succeeded.
2015-02-10 08:47:31.515186 [info] Test "CYCLIC_REQUEST_DELAY_TEST_1234_0023" succeeded.
2015-02-10 08:47:31.515325 [info] Test "MIN_INITIAL_DELAY_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.515395 [info] Test "MAX_INITIAL_DELAY_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.515536 [info] Test "REPETITION_BASE_DELAY_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.515691 [info] Test "REPETITION_MAX_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.515834 [info] Test "CYCLIC_OFFER_DELAY_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.515971 [info] Test "CYCLIC_REQUEST_DELAY_TEST_2277_0022" succeeded.
2015-02-10 08:47:31.516109 [info] Test "MIN_INITIAL_DELAY_TEST_2266_0022" succeeded.
2015-02-10 08:47:31.516279 [info] Test "MAX_INITIAL_DELAY_TEST_2266_0022" succeeded.
2015-02-10 08:47:31.516380 [info] Test "REPETITION_BASE_DELAY_TEST_2266_0022" succeeded.
2015-02-10 08:47:31.516512 [info] Test "REPETITION_MAX_TEST_2266_0022" succeeded.
2015-02-10 08:47:31.516610 [info] Test "CYCLIC_OFFER_DELAY_TEST_2266_0022" succeeded.
2015-02-10 08:47:31.516736 [info] Test "CYCLIC_REQUEST_DELAY_TEST_2266_0022" succeeded.
2015-02-10 08:47:31.516874 [info] Test "ADDRESS_TEST_4466_0321" succeeded.
2015-02-10 08:47:31.516974 [info] Test "SERVICE DISCOVERY PROTOCOL" succeeded.
2015-02-10 08:47:31.517106 [info] Test "SERVICE DISCOVERY PORT" succeeded.


Magic Cookies Test
------------------
To run the magic cookies test you need two devices on the same network. The network addresses within
the configuration files need to be adapted to match the devices addresses. 

To start the magic-cookies-test from the build-directory do:

HOST1: 
env VSOMEIP_CONFIGURATION_FILE=../config/vsomeip-magic-cookies-client.json \
VSOMEIP_APPLICATION_NAME=client-sample ./magic-cookies-test-client

HOST2: 
env VSOMEIP_CONFIGURATION_FILE=../config/vsomeip-magic-cookies-service.json \
VSOMEIP_APPLICATION_NAME=service-sample ./response-sample --tcp --static-routing

The expected result is an output like this on service side:

2015-02-10 08:42:07.317695 [info] Received a message with Client/Session [1343/0001]
2015-02-10 08:42:07.360105 [error] Detected Magic Cookie within message data. Resyncing.
2015-02-10 08:42:07.360298 [info] Received a message with Client/Session [1343/0003]
2015-02-10 08:42:07.360527 [error] Detected Magic Cookie within message data. Resyncing.
2015-02-10 08:42:07.360621 [error] Detected Magic Cookie within message data. Resyncing.
2015-02-10 08:42:07.360714 [info] Received a message with Client/Session [1343/0006]
2015-02-10 08:42:07.360850 [info] Received a message with Client/Session [1343/0007]
2015-02-10 08:42:07.361021 [error] Detected Magic Cookie within message data. Resyncing.
2015-02-10 08:42:07.361107 [error] Detected Magic Cookie within message data. Resyncing.
2015-02-10 08:42:07.361191 [error] Detected Magic Cookie within message data. Resyncing.
2015-02-10 08:42:07.361276 [info] Received a message with Client/Session [1343/000b]
2015-02-10 08:42:07.361434 [info] Received a message with Client/Session [1343/000c]
2015-02-10 08:42:07.361558 [info] Received a message with Client/Session [1343/000d]
2015-02-10 08:42:07.361672 [error] Detected Magic Cookie within message data. Resyncing.
2015-02-10 08:42:07.361761 [info] Received a message with Client/Session [1343/000f]
