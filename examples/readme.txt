To use the example applications you need two devices on the same network. The network addresses within
the configuration files need to be adapted to match the devices addresses.

To start the request/response-example from the build-directory do:

HOST1: env VSOMEIP_CONFIGURATION_FILE=../config/vsomeip.xml VSOMEIP_APPLICATION_NAME=client-sample ./request-sample
HOST2: env VSOMEIP_CONFIGURATION_FILE=../config/vsomeip.xml VSOMEIP_APPLICATION_NAME=service-sample ./response-sample

To start the subscribe/notify-example from the build-directory do:

HOST1: env VSOMEIP_CONFIGURATION_FILE=../config/vsomeip.xml VSOMEIP_APPLICATION_NAME=client-sample ./subscribe-sample
HOST2: env VSOMEIP_CONFIGURATION_FILE=../config/vsomeip.xml VSOMEIP_APPLICATION_NAME=service-sample ./notify-sample
