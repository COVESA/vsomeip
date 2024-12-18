# Legal notice

## Copyright
Copyright (C) 2015-2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

## License

This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Version

This documentation was generated for version 3.5 of vSomeIP.

# Index
- [vSomeIP User Guide](#vsomeip-user-guide)
  - [vSomeIP Overview](#vsomeip-overview)
  - [Build Instructions](#build-instructions)
  - [Configuration File Structure](#configuration-file-structure)
  - [Autoconfiguration](#autoconfiguration)
  - [routingmanagerd](#routingmanagerd)
  - [Trace Connector](#trace-connector)
  - [Tools](#tools)
  - [vsomeip Hello World](#vsomeip-hello-world)

# vSomeIP Overview
----------------

The vSomeIP stack implements the http://some-ip.com/ (Scalable service-Oriented MiddlewarE over IP (SOME/IP)) Protocol.
The stack consists out of:

* a shared library for SOME/IP (`libvsomeip3.so`)
* a shared library for SOME/IP's configuration module (`libvsomeip3-cfg.so`)
* a shared library for SOME/IP's service discovery (`libvsomeip3-sd.so`)
* a shared library for SOME/IP's E2E protection module (`libvsomeip3-e2e.so`)

Optional:

* a shared library for compatibility with vsomeip v2 (`libvsomeip.so`)

# Build Instructions
----------------
## Dependencies

* A C++17 enabled compiler is needed
* vSomeIP uses cmake as buildsystem.
* vSomeIP uses Boost >= 1.66:
    * Ubuntu 22.04:
        * `sudo apt-get install libboost-system1.74-dev libboost-thread1.74-dev libboost-log1.74-dev`
* For the tests Google's test framework
    https://github.com/google/googletest/releases
* To build the documentation doxygen and graphviz are needed:
    * `sudo apt-get install doxygen graphviz`

## Compilation

For compilation call:

```bash
mkdir build
cd build
cmake ..
make
```

To specify a installation directory (like `--prefix=` if you're used to autotools) call cmake like:

```bash
cmake -DCMAKE_INSTALL_PREFIX:PATH=$YOUR_PATH ..
make
make install
```

### Compilation with predefined base path

To predefine the base path, the path that is used to create the local sockets, call cmake like:

```bash
cmake -DBASE_PATH=<YOUR BASE PATH> ..
```

The default base path is `/tmp`.

### Compilation with predefined unicast and/or diagnosis address

To predefine the unicast address, call cmake like:

```bash
cmake -DUNICAST_ADDRESS=<YOUR IP ADDRESS> ..
```

To predefine the diagnosis address, call cmake like:

```bash
cmake -DDIAGNOSIS_ADDRESS=<YOUR DIAGNOSIS ADDRESS> ..
```

The diagnosis address is a single byte value.

### Compilation with custom default configuration folder

To change the default configuration folder, call cmake like:

```bash
cmake -DDEFAULT_CONFIGURATION_FOLDER=<DEFAULT CONFIGURATION FOLDER> ..
```

The default configuration folder is `/etc/vsomeip`.

### Compilation with custom default configuration file

To change the default configuration file, call cmake like:

```bash
cmake -DDEFAULT_CONFIGURATION_FILE=<DEFAULT CONFIGURATION FILE> ..
```

The default configuration file is `/etc/vsomeip.json`.

### Compilation with changed (maximimum) wait times for local TCP ports

If local communication is done via TCP, we depend on the availability of the network.
Therefore, server port creation and therefore port assignment might fail. If the failure is causes by the port already being in use, we use the next available port.
For all other failure, we wait for a wait time until we retry with the same port. If the overall wait time of all retries exceeds the maximum wait time, the endpoint creation is aborted.
To configure wait time and maximum wait time, call cmake with:

```bash
cmake -DLOCAL_TCP_PORT_WAIT_TIME=50 -DLOCAL_TCP_PORT_MAX_WAIT_TIME=2000 ..
```

The `default values are a wait time of 100ms` and a `maximum wait time of 10000ms`.
These configurations have no effect if local communication uses UDS (default).

### Compilation with signal handling

To compile vsomeip with signal handling (SIGINT/SIGTERM) enabled, call cmake like:

```bash
cmake -DENABLE_SIGNAL_HANDLING=1 ..
```

In the default setting, the application has to take care of shutting down vsomeip in case these signals are received.

### Compilation with user defined "READY" message

To compile vsomeip with a user defined message signal the IP routing to be ready to send/receive messages, call cmake like:

```bash
cmake -DROUTING_READY_MESSAGE=<YOUR MESSAGE> ..
```


### Compilation with vSomeIP 2 compatibility layer

To compile vsomeip with enabled vSomeIP 2 compatibility layer, call cmake like:

```bash
cmake -DENABLE_COMPAT=1 ..
```

### Compilation of examples

For compilation of the examples call:

```bash
mkdir build
cd build
cmake ..
make examples
```

### Compilation of tests

To compile the tests, first unzip gtest to location of your desire. Some of the tests require a second node on the same network.
There are two cmake variables which are used to automatically adapt the json files to the used network setup:

* `TEST_IP_MASTER`: The IP address of the interface which will act as test master.
* `TEST_IP_SLAVE`: The IP address of the interface of the second node which will act as test slave.

If one of this variables isn't specified, only the tests using local communication exclusively will be runnable.

Additionally the unit tests require enabled signal handling which can be enabled via the `ENABLE_SIGNAL_HANDLING` cmake variable.

**Example, compilation of tests**:

```bash
mkdir build
cd build
export GTEST_ROOT=$PATH_TO_GTEST/gtest-1.7.0/
cmake -DENABLE_SIGNAL_HANDLING=1 -DTEST_IP_MASTER=10.0.3.1 -DTEST_IP_SLAVE=10.0.3.125 ..
make check
```

Additional make targets for the tests:

* Call `make build_tests` to only compile the tests
* Call `ctest` in the build directory to execute the tests without a verbose output
* To run single tests call `ctest --verbose --tests-regex $TESTNAME` short form: `ctest -V -R $TESTNAME`
* To list all available tests run `ctest -N`.
* For further information about the tests please have a look at the `readme.txt` in the `test` subdirectory.

For development purposes two cmake variables exist which control if the json files and test scripts are copied (default) or symlinked into the build directory.
These settings are ignored on Windows.

* `TEST_SYMLINK_CONFIG_FILES`: Controls if the json and scripts needed to run the tests are copied or symlinked into the build directory.
  (`Default`: OFF, ignored on Windows)
* `TEST_SYMLINK_CONFIG_FILES_RELATIVE`: Controls if the json and scripts needed to run the tests are symlinked relatively into the build directory.
  (`Default`: OFF, ignored on Windows)

**Example cmake call**:

```bash
cmake  -DTEST_SYMLINK_CONFIG_FILES=ON -DTEST_SYMLINK_CONFIG_FILES_RELATIVE=ON ..
```

For compilation of only a subset of tests (for a quick functionality check) the cmake variable `TESTS_BAT` has to be set:

**Example cmake call**:
```bash
cmake  -DTESTS_BAT=ON ..
```

### Generating the documentation

To generate the documentation call cmake as described in [Compilation](#compilation) and then call `make doc`.
This will generate:

* The README file in html: `$BUILDDIR/documentation/README.html`
* A doxygen documentation in `$BUILDDIR/documentation/html/index.html`


# Configuration File Structure
----------------
The configuration files for vsomeip are [JSON](http://www.json.org)-Files and are composed out of multiple key value pairs and arrays.

* An object is an unordered set of name/value pairs. An object begins with `{ (left brace)` and ends with `} (right brace)`.
Each name is followed by `: (colon)` and the name/value pairs are separated by `, (comma)`.

* An array is an ordered collection of values. An array begins with `[ (leftbracket)` and ends with `] (right bracket)`.
Values are separated by `, (comma)`.

* A value can be a _string_ in double quotes, or a _number_, or `true` or `false` or `null`, or an _object_ or an _array_.
  These structures can be nested.

Please refer to **vsomeipConfiguration.md**, for a complete explanation of available configuration.


# Autoconfiguration
----------------
vsomeip supports the automatic configuration of client identifiers and the routing.
The first application that starts using vsomeip will automatically become the routing manager if it is **not** explicitly configured.
The client identifiers are generated from the diagnosis address that can be specified by defining **DIAGNOSIS_ADDRESS** when compiling vsomeip, and will use the diagnosis address as the high byte and enumerate the connecting applications within the low byte of the client identifier.

Autoconfiguration of client identifiers isn't meant to be used together with vsomeip Security.
Every client running locally needs to have at least its own credentials configured when security is activated to ensure the credential checks can pass.
Practically that means if a client requests its identifier over the autoconfiguration for which no credentials are configured (at least it isn't known which client identifier is used beforehand) it is impossible for that client to establish a connection to a server endpoint.
However if the credentials for all clients are same it's possible to configure them for the overall (or DIAGNOSIS_ADDRESS) client identifier range to mix autoconfiguration together with activated security.


# routingmanagerd
----------------
The routingmanagerd is a minimal vsomeip application intended to offer routing manager functionality on a node where one system wide configuration file is present.
It can be found in the examples folder.

**Example**: Starting the daemon on a system where the system wide configuration is stored under `/etc/vsomeip.json`:

```bash
VSOMEIP_CONFIGURATION=/etc/vsomeip.json ./routingmanagerd
```

When using the daemon it should be ensured that:

* In the system wide configuration file the routingmanagerd is defined as routing manager, meaning it contains the line `"routing" : "routingmanagerd"`.
* If the default name is overridden the entry has to be adapted accordingly.
* The system wide configuration file should contain the information about all other offered services on the system as well.
* There's no other vsomeip configuration file used on the system which contains a `"routing"` entry. As there can only be one routing manager per system.


# Trace Connector

The Trace Connector is used to forward the internal messages that are sent over the Unix Domain Sockets to DLT.
Thus, it requires that DLT is installed and the DLT module can be found in the context of CMake.


## Dynamic Configuration

The Trace Connector can also be configured dynamically over its interfaces.
You need to include '<vsomeip/trace.hpp>' to access its public interface.

### Example:

```c++
    // get trace connector
    std::shared_ptr<vsomeip::trace::connector> its_connector
    = vsomeip::trace::connector::get();

    // add channel
    std::shared_ptr<vsomeip::trace::channel> its_channel
    = its_connector->create_channel("MC", "My channel");

    // add filter rule
    vsomeip::trace::match_t its_match
        = std::make_tuple(0x1234, 0xffff, 0x80e8);
    vsomeip::trace::filter_id_t its_filter_id
    = its_channel->add_filter(its_match, true);

    // init trace connector
    its_connector->init();

    // enable trace connector
    its_connector->set_enabled(true);

    // remove the filter
    its_channel->remove_filter(its_filter_id);
```


# Tools
## vsomeip_ctrl

`vsomeip_ctrl` is a small utility which can be used to send SOME/IP messages from the commandline.
If a response arrives within 5 seconds the response will be printed.

* The instance id of the target service has to be passed in hexadecimal notation.
* The complete message has to be passed in hexadecimal notation.
* See the `--help` parameter for available options.
* If `vsomeip_ctrl` is used to send messages to a remote service and no `routingmanagerd` is running on the local machine, make sure to pass a json configuration file where `vsomeip_ctrl` is set as routing manager via environment variable.
* If `vsomeip_ctrl` is used to send messages to a local service and no `routingmanagerd` is running on the local machine, make sure to use the same json configuration file as the local service.

For compilation of the [vsomeip_ctrl](#vsomeip_ctrl) utility call:

```bash
mkdir build
cd build
cmake ..
make vsomeip_ctrl
```

**Example**: Calling method with method id 0x80e8 on service with service id 0x1234, instance id 0x5678:

```bash
./vsomeip_ctrl --instance 5678 --message 123480e800000015134300030100000000000009efbbbf576f726c6400
```

**Example**: Sending a message to service with service id 0x1234, instance id 0x5678 and method id 0x0bb8 via TCP

```bash
./vsomeip_ctrl --tcp --instance 5678 --message 12340bb8000000081344000101010000
```

## vsomeip-dissector
Wireshark plugin dissector for vSomeip internal communication via TCP

### How To Use

1. Place `vsomeip-dissector.lua` file in `~/.config/wireshark/plugins/vsomeip/vsomeip-dissector.lua` 
   (create `plugins` directory if it doesn't exist)
2. In wireshark go to `Analyze` > `Reload Lua Plugins`
3. In wireshark go to `Analyze` > `Enable Protocols` and search for `vsomeip` and enable it


# vsomeip Hello World

The vsomeip Hello World consists in a client and a service.
The client sends a message containing a string to the service.
The service appends the received string to the string `Hello` and sends it back to the client.
Upon receiving a response from the service the client prints the payload of the response ("Hello World").
This example is intended to be run on the same host.

All files listed here are contained in the `examples\hello_world` subdirectory.

## Build instructions

The example can build with its own CMakeFile, please compile the vsomeip stack beforehand as described in [Compilation](#compilation).
Then compile the example starting from the repository root directory as followed:

```bash
cd examples/hello_world
mkdir build
cd build
cmake ..
make
```

## Starting and expected output
### Service - Starting and expected output

```bash
$ VSOMEIP_CONFIGURATION=../helloworld-local.json \
  VSOMEIP_APPLICATION_NAME=hello_world_service \
  ./hello_world_service
2015-04-01 11:31:13.248437 [info] Using configuration file: ../helloworld-local.json
2015-04-01 11:31:13.248766 [debug] Routing endpoint at /tmp/vsomeip-0
2015-04-01 11:31:13.248913 [info] Service Discovery disabled. Using static routing information.
2015-04-01 11:31:13.248979 [debug] Application(hello_world_service, 4444) is initialized.
2015-04-01 11:31:22.705010 [debug] Application/Client 5555 got registered!
```

### Client - Starting and expected output

```bash
$ VSOMEIP_CONFIGURATION=../helloworld-local.json \
  VSOMEIP_APPLICATION_NAME=hello_world_client \
  ./hello_world_client
2015-04-01 11:31:22.704166 [info] Using configuration file: ../helloworld-local.json
2015-04-01 11:31:22.704417 [debug] Connecting to [0] at /tmp/vsomeip-0
2015-04-01 11:31:22.704630 [debug] Listening at /tmp/vsomeip-5555
2015-04-01 11:31:22.704680 [debug] Application(hello_world_client, 5555) is initialized.
Sending: World
Received: Hello World
```

## CMakeFile

[examples/hello_world/CMakeLists.txt](../examples/hello_world/CMakeLists.txt)

## Configuration File For Client and Service

[examples/hello_world/helloworld-local.json](../examples/hello_world/helloworld-local.json)

## Service

[examples/hello_world/hello_world_service_main.cpp](../examples/hello_world/hello_world_service_main.cpp)

The service example results in the following program execution:

### Main

1. *main()*

    First the application is initialized. After the initialization is finished the application is started.

### Initialization

2. *init()*

    The initialization contains the registration of a message handler and an event handler.

    The message handler declares a callback (__on_message_cbk__) for messages that are sent to the specific service (specifying the service id, the service instance id and the service method id).

    The event handler declares a callback (__on_event_cbk__) for events that occur. One event can be the successful registration of the application at the runtime.

### Start

3. *start()*

    The application will be started. This function only returns when the application will be stopped.

### Callbacks

4. *on_state_cbk()*

    This function is called by the application when an state change occurred. If the application was successfully registered at the runtime then the specific service is offered.

5. *on_message_cbk()*

    This function is called when a message/request from a client for the specified service was received.

    First a response based upon the request is created.
    Afterwards the string 'Hello' will be concatenated with the payload of the client's request.
    After that the payload of the response is created. The payload data is set with the previously concatenated string.
    Finally the response is sent back to the client and the application is stopped.

### Stop

6. *stop()*

    This function stops offering the service, unregister the message and the event handler and shuts down the application.

## Client

[examples/hello_world/hello_world_client_main.cpp](../examples/hello_world/hello_world_client_main.cpp)

The client example results in the following program execution:

### Main

1. *main()*

    First the application is initialized. After the initialization is finished the application is started.

### Initialization

2. *init()*

    The initialization contains the registration of a message handler, an event handler and an availability handler.

    The event handler declares again a callback (__on_state_cbk__) for state changes that occur.

    The message handler declares a callback (__on_message_cbk__) for messages that are received from any service, any service instance and any method.

    The availability handler declares a callback (__on_availability_cbk__) which is called when the specific service is available (specifying the service id and the service instance id).

### Start

3. *start()*

    The application will be started. This function only returns when the application will be stopped.

### Callbacks

4. *on_state_cbk()*

    This function is called by the application when an state change occurred.
    If the application was successfully registered at the runtime then the specific service is requested.

5. *on_availability_cbk()*

    This function is called when the requested service is available or no longer available.

    First there is a check if the change of the availability is related to the 'hello world service' and the availability changed to true.
    If the check is successful a service request is created and the appropriate service information are set (service id, service instance id, service method id).
    After that the payload of the request is created. The data of the payload is 'World' and will be set afterwards.
    Finally the request is sent to the service.

6. *on_message_cbk()*

    This function is called when a message/response was received.
    If the response is from the requested service, of type 'RESPONSE' and the return code is 'OK' then the payload of the response is printed. Finally the application is stopped.

### Stop

7. *stop()*

This function unregister the event and the message handler and shuts down the application.

