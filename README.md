### vsomeip

##### Copyright
Copyright (C) 2015-2017, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

##### License

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

##### vsomeip Overview
----------------
The vsomeip stack implements the http://some-ip.com/ (Scalable service-Oriented
MiddlewarE over IP (SOME/IP)) protocol. The stack consists out of:

* a shared library for SOME/IP (`libvsomeip.so`)
* a second shared library for SOME/IP's service discovery (`libvsomeip-sd.so`)
  which is loaded during runtime if the service discovery is enabled.

##### Build Instructions

###### Dependencies

- A C++11 enabled compiler like gcc >= 4.8 is needed.
- vsomeip uses CMake as buildsystem.
- vsomeip uses Boost >= 1.55:

Ubuntu 14.04:

`sudo apt-get install libboost-system1.55-dev libboost-thread1.55-dev libboost-log1.55-dev`

Ubuntu 12.04: a PPA is necessary to use version 1.54 of Boost:
-- URL: https://launchpad.net/~boost-latest/+archive/ubuntu/ppa
--`sudo add-apt-repository ppa:boost-latest/ppa`
--`sudo apt-get install libboost-system1.55-dev libboost-thread1.55-dev
    libboost-log1.55-dev`

For the tests Google's test framework https://code.google.com/p/googletest/[gtest] in version 1.7.0 is needed.
-- URL: https://googletest.googlecode.com/files/gtest-1.7.0.zip

To build the documentation asciidoc, source-highlight, doxygen and graphviz is needed:
--`sudo apt-get install asciidoc source-highlight doxygen graphviz`

###### Compilation

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

###### Compilation with predefined unicast and/or diagnosis address
To predefine the unicast address, call cmake like:
```bash
cmake -DUNICAST_ADDRESS=<YOUR IP ADDRESS> ..
```

To predefine the diagnosis address, call cmake like:
```bash
cmake -DDIAGNOSIS_ADDRESS=<YOUR DIAGNOSIS ADDRESS> ..
```
The diagnosis address is a single byte value.

###### Compilation with signal handling

To compile vsomeip with signal handling (SIGINT/SIGTERM) enabled, call cmake like:
```bash
cmake -DENABLE_SIGNAL_HANDLING=1 ..
```
In the default setting, the application has to take care of shutting down vsomeip in case these signals are received.
